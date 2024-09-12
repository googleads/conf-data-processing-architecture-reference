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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.aws;

import static org.mockito.Mockito.spy;

import com.google.acai.Acai;
import com.google.acai.TestScoped;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.crypto.tink.signature.EcdsaSignKeyManager;
import com.google.crypto.tink.signature.SignatureConfig;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.EncryptionKeySignatureKey;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTaskBase;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.SplitKeyGenerationTestEnv;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdFactory;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.UuidKeyIdFactory;
import java.security.GeneralSecurityException;
import java.util.Optional;
import org.junit.Rule;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Test the full CreateSplitKeyTask using a faked KeyStorageClient and spied-on dependencies. */
@RunWith(JUnit4.class)
public class AwsCreateSplitKeyTaskTest extends AwsCreateSplitKeyTaskTestBase {

  @Rule public final Acai acai = new Acai(TestEnv.class);

  private static class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      install(new SplitKeyGenerationTestEnv());
      bind(CreateSplitKeyTaskBase.class).to(AwsCreateSplitKeyTask.class);
      bind(KeyIdFactory.class).toInstance(new UuidKeyIdFactory());
    }

    @Provides
    @TestScoped
    KeysetHandle provideKeysetHandle() throws GeneralSecurityException {
      SignatureConfig.register();
      return KeysetHandle.generateNew(EcdsaSignKeyManager.ecdsaP256Template());
    }

    @Provides
    @TestScoped
    PublicKeySign providePublicKeySign(KeysetHandle signatureKey) throws GeneralSecurityException {
      return spy(signatureKey.getPrimitive(PublicKeySign.class));
    }

    @Provides
    @TestScoped
    PublicKeyVerify providePublicKeyVerify(KeysetHandle signatureKey)
        throws GeneralSecurityException {
      return signatureKey.getPublicKeysetHandle().getPrimitive(PublicKeyVerify.class);
    }

    @Provides
    @TestScoped
    @EncryptionKeySignatureKey
    Optional<PublicKeySign> providePublicKeyVerifyOptional(PublicKeySign publicKeySign) {
      return Optional.of(publicKeySign);
    }
  }
}
