/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keystorage.service.aws;

import static com.google.common.collect.ImmutableMap.toImmutableMap;
import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceBaseUrl;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient.KeyStorageServiceException;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableMap;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsPublicKeyVerify;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.CoordinatorBHttpClient;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.HttpKeyStorageClient;
import com.google.scp.coordinator.keymanagement.keystorage.service.aws.testing.KeyStorageServiceKeysProviderModule;
import com.google.scp.coordinator.keymanagement.keystorage.service.aws.testing.LocalKeyStorageServiceModule;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKekUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureAlgorithm;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureKeyId;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.shared.util.KeySplitDataUtil;
import com.google.scp.coordinator.keymanagement.testutils.FakeDataKeyUtil;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.KeyDbIntegrationTestModule;
import com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorBKeyDbTableName;
import com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorBWorkerKekUri;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestModule;
import com.google.scp.shared.testutils.aws.LocalStackAwsClientUtil;
import java.security.GeneralSecurityException;
import java.time.Instant;
import java.util.Base64;
import java.util.Optional;
import javax.inject.Inject;
import javax.inject.Singleton;
import org.apache.http.client.HttpClient;
import org.apache.http.impl.client.HttpClients;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.services.kms.KmsClient;

/**
 * Integration test that tests the compatibility of {@link HttpKeyStorageClient} against a
 * LocalStack-deployed version of the KeyStorageService lambda.
 */
@RunWith(JUnit4.class)
public final class CreateKeyApiGatewayHandlerIntegrationTest {

  @Rule public Acai acai = new Acai(TestEnv.class);
  @Inject @KeyStorageServiceBaseUrl String baseUrl;
  @Inject @CoordinatorBWorkerKekUri String coordinatorBKeyUri;
  @Inject @CoordinatorKekUri String dataKeyKekUri;
  @Inject HttpKeyStorageClient client;
  @Inject KmsClient kmsClient;
  @Inject @EncryptionKeySignatureKeyId Optional<String> encryptionKeySignatureKeyId;
  @Inject @EncryptionKeySignatureAlgorithm String encryptionKeySignatureAlgorithm;
  // CoordinatorBKeyDbTableName and DynamoKeyDbTableName are the same so the injected DynamoKeydb
  // points to Coordinator B's keydb.
  @Inject DynamoKeyDb coordinatorBKeyDb;
  @Inject CloudAeadSelector aeadSelector;

  @Test
  public void createKey_invalidKeySplit() throws Exception {
    var encryptionKey = FakeEncryptionKey.create();
    var invalidKeySplit = "";

    assertThat(coordinatorBKeyDb.getAllKeys().size()).isEqualTo(0);

    var exception =
        assertThrows(
            KeyStorageServiceException.class,
            () -> client.createKey(encryptionKey, invalidKeySplit));

    var serviceException = (ServiceException) exception.getCause();
    assertThat(serviceException.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    // No key should have been saved.
    assertThat(coordinatorBKeyDb.getAllKeys().size()).isEqualTo(0);
  }

  @Test
  public void createKey_success() throws Exception {
    FakeDataKeyUtil.registerDataKey(aeadSelector.getAead(dataKeyKekUri), dataKeyKekUri);
    var dataKey = client.fetchDataKey();
    var encryptionKey = FakeEncryptionKey.create();
    var keyId = encryptionKey.getKeyId();
    var encryptedKeySplit =
        FakeDataKeyUtil.encryptString(
            dataKey, "fake-key-split", encryptionKey.getPublicKeyMaterial());

    ImmutableMap<String, PublicKeyVerify> fakeKeyVerifiers =
        encryptionKey.getKeySplitDataList().stream()
            .filter(keySplitData -> !keySplitData.getPublicKeySignature().isEmpty())
            .collect(
                toImmutableMap(
                    KeySplitData::getKeySplitKeyEncryptionKeyUri,
                    keySplitData -> FakeEncryptionKey.PUBLIC_KEY_VERIFY));
    var publicKeyVerify =
        new AwsKmsPublicKeyVerify(
            kmsClient, encryptionKeySignatureKeyId.get(), encryptionKeySignatureAlgorithm);
    ImmutableMap<String, PublicKeyVerify> publicKeyVerifiers =
        ImmutableMap.<String, PublicKeyVerify>builder()
            .put(coordinatorBKeyUri, publicKeyVerify)
            .putAll(fakeKeyVerifiers)
            .build();

    assertThat(coordinatorBKeyDb.getAllKeys().size()).isEqualTo(0);

    client.createKey(encryptionKey, dataKey, encryptedKeySplit);

    assertThat(coordinatorBKeyDb.getAllKeys().size()).isEqualTo(1);
    var key = coordinatorBKeyDb.getKey(keyId);
    assertThat(key.getKeyEncryptionKeyUri()).isEqualTo(coordinatorBKeyUri);
    assertThat(getDecryptedKeyMaterial(key)).isEqualTo("fake-key-split".getBytes());
    // will throw an exception if any of the signatures are invalid
    KeySplitDataUtil.verifyEncryptionKeySignatures(key, publicKeyVerifiers);
  }

  // TODO(b/298102431)
  @Ignore("Flaky test with timeout errors.")
  @Test
  public void createKey_failsIfSpoofedDataKey() throws Exception {
    FakeDataKeyUtil.registerDataKey(aeadSelector.getAead(dataKeyKekUri), dataKeyKekUri);
    var dataKey = client.fetchDataKey();
    var keyId = "test-key-id-2";
    var encryptionKey = FakeEncryptionKey.create().toBuilder().setKeyId(keyId).build();
    var encryptedKeySplit =
        FakeDataKeyUtil.encryptString(
            dataKey, "fake-key-split", encryptionKey.getPublicKeyMaterial());

    var invalidSignatureDataKey =
        // Note: must be valid Base64 and a valid length otherwise a client error will be received.
        dataKey.toBuilder()
            .setMessageAuthenticationCode(
                "MEUCIQDSvG/JZoelpSEE0ijilFGvoOvjWP5JdyuR1WPxC93eWQIgYFUPPf7cs3awjTaJdKGGNObqXIH/uIWUqLvEfplPhZk=")
            .build();
    var invalidContextDataKey =
        dataKey.toBuilder().setDataKeyContext(Instant.now().plusSeconds(100).toString()).build();

    var ex1 =
        assertThrows(
            KeyStorageServiceException.class,
            () -> client.createKey(encryptionKey, invalidSignatureDataKey, encryptedKeySplit));
    var ex2 =
        assertThrows(
            KeyStorageServiceException.class,
            () -> client.createKey(encryptionKey, invalidContextDataKey, encryptedKeySplit));

    assertThat(ex1).hasCauseThat().hasMessageThat().contains("Signature validation failed");
    assertThat(ex2).hasCauseThat().hasMessageThat().contains("Signature validation failed");
    assertThat(coordinatorBKeyDb.getAllKeys().size()).isEqualTo(0);
  }

  @Test
  public void fetchDataKey_success() throws Exception {
    FakeDataKeyUtil.registerDataKey(aeadSelector.getAead(dataKeyKekUri), dataKeyKekUri);

    var dataKey = client.fetchDataKey();

    assertThat(dataKey.getEncryptedDataKey()).isNotEmpty();
    assertThat(dataKey.getEncryptedDataKeyKekUri()).isEqualTo(dataKeyKekUri);
    assertThat(dataKey.getDataKeyContext()).isNotEmpty();
    assertThat(dataKey.getMessageAuthenticationCode()).isNotEmpty();

    // Verify key is trusted by createKey.
    var encryptionKey = FakeEncryptionKey.create();
    var encryptedKeySplit =
        FakeDataKeyUtil.encryptString(dataKey, "blah", encryptionKey.getPublicKeyMaterial());
    client.createKey(encryptionKey, dataKey, encryptedKeySplit);
  }

  @Test
  public void createKey_failsIfAssociatedDataNotMatch() throws Exception {
    FakeDataKeyUtil.registerDataKey(aeadSelector.getAead(dataKeyKekUri), dataKeyKekUri);
    var dataKey = client.fetchDataKey();
    var encryptionKey =
        FakeEncryptionKey.create().toBuilder().setPublicKeyMaterial("public/key==").build();
    var encryptedKeySplit = FakeDataKeyUtil.encryptString(dataKey, "blah", "wrong/public/key");

    var ex =
        assertThrows(
            KeyStorageServiceException.class,
            () -> client.createKey(encryptionKey, dataKey, encryptedKeySplit));
    assertThat(ex).hasCauseThat().hasMessageThat().contains("Failed to decrypt using data key");
  }

  private byte[] getDecryptedKeyMaterial(EncryptionKey encryptionKey)
      throws GeneralSecurityException {
    var decrypter = aeadSelector.getAead(encryptionKey.getKeyEncryptionKeyUri());
    var encryptedBytes = Base64.getDecoder().decode(encryptionKey.getJsonEncodedKeyset());
    return decrypter.decrypt(encryptedBytes, null);
  }

  public static class TestEnv extends AbstractModule {

    static {
      try {
        HybridConfig.register();
      } catch (GeneralSecurityException e) {
        throw new RuntimeException("Error initializing tink.");
      }
    }

    @Provides
    @Singleton
    public CloudAeadSelector provideAeadSelector(LocalStackContainer localStack) {
      return LocalStackAwsClientUtil.createCloudAeadSelector(localStack);
    }

    /**
     * Map the table created by {@link KeyDbIntegrationTestModule} to the table expected by {@link
     * LocalKeyStorageServiceModule}
     */
    @Provides
    @CoordinatorBKeyDbTableName
    public String provideKeydbTableName(@DynamoKeyDbTableName String tableName) {
      return tableName;
    }

    @Override
    public void configure() {
      bind(HttpClient.class)
          .annotatedWith(CoordinatorBHttpClient.class)
          .toInstance(HttpClients.createDefault());
      install(new AwsIntegrationTestModule());
      install(new KeyDbIntegrationTestModule());
      install(new KeyStorageServiceKeysProviderModule());
      install(new LocalKeyStorageServiceModule());
    }
  }
}
