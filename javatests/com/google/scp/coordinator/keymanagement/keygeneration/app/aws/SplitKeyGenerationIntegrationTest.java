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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAEncryptionKeySignatureAlgorithm;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAEncryptionKeySignatureKeyId;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAWorkerKekUri;

import com.google.acai.Acai;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.inject.AbstractModule;
import com.google.inject.Key;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing.SplitKeyGenerationArgsLocalStackProvider;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyIdTypeName;
import com.google.scp.coordinator.keymanagement.keystorage.service.aws.testing.KeyStorageServiceKeysProviderModule;
import com.google.scp.coordinator.keymanagement.keystorage.service.aws.testing.LocalKeyStorageServiceModule;
import com.google.scp.coordinator.keymanagement.testutils.aws.MultiKeyDbIntegrationTestModule;
import com.google.scp.coordinator.keymanagement.testutils.aws.TestKeyGenerationQueueModule;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestModule;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import java.security.GeneralSecurityException;
import java.util.Optional;
import javax.inject.Singleton;
import org.apache.http.client.HttpClient;
import org.apache.http.impl.client.HttpClients;
import org.junit.Rule;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.services.kms.KmsClient;

/**
 * Integration tests which test the critical journeys of the SplitKeyGeneration process
 *
 * <p>These tests run locally using LocalStack for AWS services and deploys a real KeyStorageService
 * there.
 *
 * <p>SplitKeyGeneration is run inside the Java test process as an isolated Guice module tree
 * started using command line arguments via {@ link SplitKeyGenerationStart}
 */
@RunWith(JUnit4.class)
public final class SplitKeyGenerationIntegrationTest extends SplitKeyGenerationIntegrationTestBase {
  @Rule public final Acai acai = new Acai(TestEnv.class);

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
    @CoordinatorAWorkerKekUri
    public String provideKekA(KmsClient kmsClient) {
      var keyId = AwsIntegrationTestUtil.createKmsKey(kmsClient);
      return String.format("aws-kms://arn:aws:kms:us-east-1:000000000000:key/%s", keyId);
    }

    @Provides
    @Singleton
    @CoordinatorAEncryptionKeySignatureKeyId
    public Optional<String> provideSignatureKey(KmsClient kmsClient) {
      return Optional.of(AwsIntegrationTestUtil.createSignatureKey(kmsClient));
    }

    @Override
    public void configure() {
      bind(HttpClient.class).toInstance(HttpClients.createDefault());
      bind(String.class)
          .annotatedWith(CoordinatorAEncryptionKeySignatureAlgorithm.class)
          .toInstance(AwsIntegrationTestUtil.SIGNATURE_KEY_ALGORITHM);
      bind(new Key<Optional<String>>(KeyIdTypeName.class) {}).toInstance(Optional.empty());
      install(new AwsIntegrationTestModule());
      install(new TestKeyGenerationQueueModule());
      install(new MultiKeyDbIntegrationTestModule());
      install(new KeyStorageServiceKeysProviderModule());
      install(new LocalKeyStorageServiceModule());
      install(new SplitKeyGenerationArgsLocalStackProvider());
    }
  }
}
