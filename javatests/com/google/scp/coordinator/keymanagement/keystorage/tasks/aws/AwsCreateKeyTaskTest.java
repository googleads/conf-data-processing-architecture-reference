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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.aws;

import static com.google.common.collect.ImmutableMap.toImmutableMap;
import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static org.junit.Assert.assertThrows;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableMap;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.crypto.tink.signature.EcdsaSignKeyManager;
import com.google.crypto.tink.signature.SignatureConfig;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.SignDataKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.shared.util.KeySplitDataUtil;
import com.google.scp.coordinator.keymanagement.testutils.FakeDataKeyUtil;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import java.security.GeneralSecurityException;
import java.util.Base64;
import java.util.Optional;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import software.amazon.awssdk.services.kms.model.SignRequest;

@RunWith(JUnit4.class)
public class AwsCreateKeyTaskTest {
  private static final CloudAeadSelector aeadSelector = FakeDataKeyUtil.getAeadSelector();
  private static final String KEK_URI = "fake-kek-uri";

  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Inject private InMemoryKeyDb keyDb;
  @Mock private Aead mockAead;
  @Mock private SignDataKeyTask signDataKeyTask;
  private PublicKeyVerify publicKeyVerify;

  // Under test.
  AwsCreateKeyTask task;

  @Before
  public void setup() throws GeneralSecurityException {
    SignatureConfig.register();

    KeysetHandle signatureKey = KeysetHandle.generateNew(EcdsaSignKeyManager.ecdsaP256Template());
    PublicKeySign publicKeySign = signatureKey.getPrimitive(PublicKeySign.class);
    publicKeyVerify = signatureKey.getPublicKeysetHandle().getPrimitive(PublicKeyVerify.class);
    task =
        new AwsCreateKeyTask(
            keyDb, mockAead, KEK_URI, Optional.of(publicKeySign), aeadSelector, signDataKeyTask);
  }

  @Test
  public void createKey_throwsIfNoDataKey() {
    var serviceException =
        assertThrows(
            ServiceException.class, () -> task.createKey(FakeEncryptionKey.create(), "foo"));

    assertThat(serviceException)
        .hasMessageThat()
        .contains("incoming keys must be encrypted with a DataKey");
  }

  @Test
  public void createKey_success() throws Exception {
    when(mockAead.encrypt(any(), any())).thenReturn("foo".getBytes());
    var dataKey = FakeDataKeyUtil.createDataKey();
    var key = FakeEncryptionKey.create().toBuilder().setPublicKey("bar").build();
    var keySplit = FakeDataKeyUtil.encryptString(dataKey, "fake-split", key.getPublicKeyMaterial());
    var arg = ArgumentCaptor.forClass(SignRequest.class);

    ImmutableMap<String, PublicKeyVerify> fakeKeyVerifiers =
        key.getKeySplitDataList().stream()
            .filter(keySplitData -> !keySplitData.getPublicKeySignature().isEmpty())
            .collect(
                toImmutableMap(
                    KeySplitData::getKeySplitKeyEncryptionKeyUri,
                    keySplitData -> FakeEncryptionKey.PUBLIC_KEY_VERIFY));
    ImmutableMap<String, PublicKeyVerify> publicKeyVerifiers =
        ImmutableMap.<String, PublicKeyVerify>builder()
            .put(KEK_URI, publicKeyVerify)
            .putAll(fakeKeyVerifiers)
            .build();

    task.createKey(key, dataKey, keySplit);

    assertThat(keyDb.getAllKeys().size()).isEqualTo(1);
    var storedKey = keyDb.getKey(key.getKeyId());
    assertThat(storedKey.getKeyEncryptionKeyUri()).isEqualTo(KEK_URI);
    assertThat(storedKey.getPublicKey()).isEqualTo("bar");
    assertThat(storedKey.getExpirationTime()).isEqualTo(key.getExpirationTime());
    assertThat(storedKey.getExpirationTime()).isEqualTo(key.getExpirationTime());
    assertThat(storedKey.getCreationTime()).isGreaterThan(0L);
    KeySplitDataUtil.verifyEncryptionKeySignatures(storedKey, publicKeyVerifiers);

    // The stored key is expected to be base 64 encoded.
    assertThat(storedKey.getJsonEncodedKeyset())
        .isEqualTo(new String(Base64.getEncoder().encode("foo".getBytes())));
    verify(mockAead).encrypt(eq("fake-split".getBytes()), any());
    verify(signDataKeyTask, times(1)).verifyDataKey(any(DataKey.class));
  }

  @Test
  public void createKey_successOverwrite() throws Exception {
    var dataKey = FakeDataKeyUtil.createDataKey();
    String fakePublicKey = "fake-public_key";
    var keySplit1 = FakeDataKeyUtil.encryptString(dataKey, "fake-split-1", fakePublicKey);
    var keySplit2 = FakeDataKeyUtil.encryptString(dataKey, "fake-split-2", fakePublicKey);
    when(mockAead.encrypt(aryEq("fake-split-1".getBytes()), any()))
        .thenReturn("key1_encrypted".getBytes());
    when(mockAead.encrypt(aryEq("fake-split-2".getBytes()), any()))
        .thenReturn("key2_encrypted".getBytes());
    String keyId = "asdf";
    var key1 =
        FakeEncryptionKey.create().toBuilder()
            .setPublicKey("old")
            .setPublicKeyMaterial(fakePublicKey)
            .setKeyId(keyId)
            .build();
    var key2 =
        FakeEncryptionKey.create().toBuilder()
            .setPublicKey("new")
            .setPublicKeyMaterial(fakePublicKey)
            .setKeyId(keyId)
            .build();

    task.createKey(key1, dataKey, keySplit1);
    task.createKey(key2, dataKey, keySplit2);

    var result = keyDb.getKey(keyId);
    assertThat(keyDb.getAllKeys().size()).isEqualTo(1);
    assertThat(result.getPublicKey()).isEqualTo("new");
    assertThat(result.getJsonEncodedKeyset())
        .isEqualTo(Base64.getEncoder().encodeToString("key2_encrypted".getBytes()));
    assertThat(result.getExpirationTime()).isEqualTo(key2.getExpirationTime());
  }

  @Test
  public void createKey_databaseException() throws Exception {
    when(mockAead.encrypt(any(), any())).thenReturn("foo".getBytes());
    var dataKey = FakeDataKeyUtil.createDataKey();
    var key = FakeEncryptionKey.create();
    var keySplit = FakeDataKeyUtil.encryptString(dataKey, "fake-split", key.getPublicKeyMaterial());

    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));

    ServiceException ex =
        assertThrows(ServiceException.class, () -> task.createKey(key, dataKey, keySplit));

    assertThat(ex.getErrorCode()).isEqualTo(INTERNAL);
  }

  @Test
  public void createKey_throwsIfEmptyDataKey() throws Exception {
    when(mockAead.encrypt(any(), any())).thenReturn("foo".getBytes());
    // create default data key.
    var dataKey = DataKey.newBuilder().build();
    var keySplit = new String(Base64.getEncoder().encode("fake-split".getBytes()));
    var key = FakeEncryptionKey.create();

    var ex = assertThrows(ServiceException.class, () -> task.createKey(key, dataKey, keySplit));

    assertThat(ex.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    assertThat(ex).hasCauseThat().isInstanceOf(GeneralSecurityException.class);
    assertThat(keyDb.getAllKeys().size()).isEqualTo(0);
    verify(mockAead, never()).encrypt(any(), any());
  }

  @Test
  public void createKey_throwsIfEmptyKeySplit() throws Exception {
    when(mockAead.encrypt(any(), any())).thenReturn("foo".getBytes());
    var dataKey = FakeDataKeyUtil.createDataKey();
    var key = FakeEncryptionKey.create();
    var keySplit = FakeDataKeyUtil.encryptString(dataKey, "", key.getPublicKeyMaterial());

    var ex = assertThrows(ServiceException.class, () -> task.createKey(key, dataKey, keySplit));

    assertThat(ex.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    assertThat(ex.getMessage()).contains("payload is empty");
    assertThat(keyDb.getAllKeys().size()).isEqualTo(0);
    verify(mockAead, never()).encrypt(any(), any());
  }

  @Test
  public void createKey_throwsIfIncorrectDataKey() throws Exception {
    when(mockAead.encrypt(any(), any())).thenReturn("foo".getBytes());
    var dataKey = FakeDataKeyUtil.createDataKey();
    // use a different data key to encrypt the key split
    var key = FakeEncryptionKey.create();
    var keySplit =
        FakeDataKeyUtil.encryptString(
            FakeDataKeyUtil.createDataKey(), "fake-split", key.getPublicKeyMaterial());

    var ex = assertThrows(ServiceException.class, () -> task.createKey(key, dataKey, keySplit));

    assertThat(ex.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    assertThat(ex.getMessage()).isEqualTo("Failed to decrypt using data key");
    assertThat(keyDb.getAllKeys().size()).isEqualTo(0);
    verify(mockAead, never()).encrypt(any(), any());
  }

  @Test
  public void createKey_throwsIfVerifyFails() throws Exception {
    var verifyFailedMessage = "~verify failed~";
    doThrow(ServiceException.ofUnknownException(new RuntimeException(verifyFailedMessage)))
        .when(signDataKeyTask)
        .verifyDataKey(any(DataKey.class));
    var dataKey = FakeDataKeyUtil.createDataKey();
    var key = FakeEncryptionKey.create().toBuilder().setPublicKey("bar").build();
    var keySplit = FakeDataKeyUtil.encryptString(dataKey, "fake-split", key.getPublicKeyMaterial());

    var ex = assertThrows(ServiceException.class, () -> task.createKey(key, dataKey, keySplit));

    assertThat(ex).hasCauseThat().hasMessageThat().isEqualTo(verifyFailedMessage);
    assertThat(keyDb.getAllKeys().size()).isEqualTo(0);
  }

  @Test
  public void createKey_noSignature() throws Exception {
    when(mockAead.encrypt(any(), any())).thenReturn("foo".getBytes());
    var dataKey = FakeDataKeyUtil.createDataKey();
    var key = FakeEncryptionKey.create().toBuilder().setPublicKey("bar").build();
    var keySplit = FakeDataKeyUtil.encryptString(dataKey, "fake-split", key.getPublicKeyMaterial());

    task =
        new AwsCreateKeyTask(
            keyDb, mockAead, KEK_URI, Optional.empty(), aeadSelector, signDataKeyTask);

    task.createKey(key, dataKey, keySplit);

    var storedKey = keyDb.getKey(key.getKeyId());
    ImmutableMap<String, KeySplitData> keySplitDataMap =
        storedKey.getKeySplitDataList().stream()
            .collect(
                toImmutableMap(
                    KeySplitData::getKeySplitKeyEncryptionKeyUri,
                    keySplitDataItem -> keySplitDataItem));
    KeySplitData keySplitData = keySplitDataMap.get(KEK_URI);
    assertThat(keySplitData.getPublicKeySignature()).isEmpty();
  }
}
