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

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeyPublicKeySign;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeyPublicKeyVerify;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureAlgorithm;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureKeyId;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.acai.TestScoped;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsPublicKeySign;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsPublicKeyVerify;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestModule;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.ZoneId;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.services.kms.KmsClient;

/** Tests AwsSignDataKeyTask against a localstack version of KMS. */
@RunWith(JUnit4.class)
public final class AwsSignDataKeyTaskIntegrationTest {
  private static final Instant NOW = Instant.ofEpochSecond(1074560000);
  @Rule public Acai acai = new Acai(TestEnv.class);

  @Inject private AwsSignDataKeyTask task;

  @Test
  public void signDataKey_roundtrip() throws Exception {
    var dataKey =
        DataKey.newBuilder().setEncryptedDataKey("beep").setEncryptedDataKeyKekUri("boop").build();

    var signedDataKey = task.signDataKey(dataKey);
    task.verifyDataKey(signedDataKey);
  }

  @Test
  public void signDataKey_failsIfExpired() throws Exception {
    var dataKey =
        DataKey.newBuilder().setEncryptedDataKey("beep").setEncryptedDataKeyKekUri("boop").build();

    var signedDataKey =
        task.signDataKey(dataKey).toBuilder()
            .setDataKeyContext(NOW.minus(Duration.ofDays(1)).toString())
            .build();

    var ex = assertThrows(ServiceException.class, () -> task.verifyDataKey(signedDataKey));

    assertThat(ex).hasMessageThat().contains("is expired");
  }

  @Test
  public void verifyDataKey_failsIfContextChanged() throws Exception {
    var signedDataKey =
        task.signDataKey(
            DataKey.newBuilder()
                .setEncryptedDataKey("beep")
                .setEncryptedDataKeyKekUri("boop")
                .build());

    var modifiedDataKey =
        signedDataKey.toBuilder().setDataKeyContext(NOW.plusSeconds(1).toString()).build();

    var ex = assertThrows(ServiceException.class, () -> task.verifyDataKey(modifiedDataKey));

    assertThat(ex).hasMessageThat().contains("Signature validation failed");
  }

  public static class TestEnv extends AbstractModule {

    @Provides
    @TestScoped
    @DataKeySignatureKeyId
    public String provideSignatureKeyId(KmsClient kmsClient) {
      return AwsIntegrationTestUtil.createSignatureKey(kmsClient);
    }

    @Provides
    @TestScoped
    @DataKeyPublicKeySign
    public PublicKeySign providePublicKeySign(
        KmsClient kmsClient,
        @DataKeySignatureKeyId String signatureKeyId,
        @DataKeySignatureAlgorithm String signatureAlgorithm) {
      return new AwsKmsPublicKeySign(kmsClient, signatureKeyId, signatureAlgorithm);
    }

    @Provides
    @TestScoped
    @DataKeyPublicKeyVerify
    public PublicKeyVerify providePublicKeyVerify(
        KmsClient kmsClient,
        @DataKeySignatureKeyId String signatureKeyId,
        @DataKeySignatureAlgorithm String signatureAlgorithm) {
      return new AwsKmsPublicKeyVerify(kmsClient, signatureKeyId, signatureAlgorithm);
    }

    @Override
    protected void configure() {
      install(new AwsIntegrationTestModule());
      bind(Clock.class).toInstance(Clock.fixed(NOW, ZoneId.of("UTC")));
      bind(String.class)
          .annotatedWith(DataKeySignatureAlgorithm.class)
          .toInstance(AwsIntegrationTestUtil.SIGNATURE_KEY_ALGORITHM);
    }
  }
}
