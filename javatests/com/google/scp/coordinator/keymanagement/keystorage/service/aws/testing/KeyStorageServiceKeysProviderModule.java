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

package com.google.scp.coordinator.keymanagement.keystorage.service.aws.testing;

import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorBWorkerKekUri;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKekUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureAlgorithm;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureKeyId;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureAlgorithm;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureKeyId;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import java.util.Optional;
import software.amazon.awssdk.services.kms.KmsClient;

/**
 * Provides the 4 keys used by KeyStorageService:
 *
 * <ul>
 *   <li>Coordinator Key Encryption Key -- encrypts data keys sent to Coordinator A.
 *   <li>Worker Key Encryption Key -- encrypts encryption keys sent to Worker.
 *   <li>Coordinator Key Sigature Key -- signs data keys sent to Coordinator A.
 *   <li>Coordinator Sigature Key -- signs public key material processed by this coordinator.
 * </ul>
 *
 * <p>Intended to be compatible with {@link LocalKeyStorageServiceModule}.
 */
public final class KeyStorageServiceKeysProviderModule extends AbstractModule {
  @Provides
  @Singleton
  @CoordinatorBWorkerKekUri
  public String provideKekB(KmsClient kmsClient) {
    var keyId = AwsIntegrationTestUtil.createKmsKey(kmsClient);
    return String.format("aws-kms://arn:aws:kms:us-east-1:000000000000:key/%s", keyId);
  }

  @Provides
  @Singleton
  @CoordinatorKekUri
  public String provideCoordinatorKekUri(KmsClient kmsClient) {
    var keyId = AwsIntegrationTestUtil.createKmsKey(kmsClient);
    return String.format("aws-kms://arn:aws:kms:us-east-1:000000000000:key/%s", keyId);
  }

  @Provides
  @Singleton
  @DataKeySignatureKeyId
  public String provideDataKeySignatureKeyId(KmsClient kmsClient) {
    return AwsIntegrationTestUtil.createSignatureKey(kmsClient);
  }

  @Provides
  @Singleton
  @EncryptionKeySignatureKeyId
  public Optional<String> provideEncryptionKeySignatureKeyId(KmsClient kmsClient) {
    return Optional.of(AwsIntegrationTestUtil.createSignatureKey(kmsClient));
  }

  @Override
  public void configure() {
    bind(String.class)
        .annotatedWith(DataKeySignatureAlgorithm.class)
        .toInstance(AwsIntegrationTestUtil.SIGNATURE_KEY_ALGORITHM);
    bind(String.class)
        .annotatedWith(EncryptionKeySignatureAlgorithm.class)
        .toInstance(AwsIntegrationTestUtil.SIGNATURE_KEY_ALGORITHM);
  }
}
