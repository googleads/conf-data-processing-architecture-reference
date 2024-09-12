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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common;

import static org.mockito.Mockito.spy;

import com.google.acai.TestScoped;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AeadConfig;
import com.google.crypto.tink.aead.AesCtrHmacAeadKeyManager;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.testing.FakeKeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.testutils.FakeDataKeyUtil;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import java.security.GeneralSecurityException;

public class SplitKeyGenerationTestEnv extends AbstractModule {

  static {
    try {
      HybridConfig.register();
      AeadConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Failed to initialize Tink", e);
    }
  }

  @Provides
  @TestScoped
  @KmsKeyAead
  public Aead provideAead() throws GeneralSecurityException {
    return spy(
        KeysetHandle.generateNew(AesCtrHmacAeadKeyManager.aes256CtrHmacSha256Template())
            .getPrimitive(Aead.class));
  }

  @Provides
  @TestScoped
  public FakeKeyStorageClient provideFakeKeyStorageClient() {
    return spy(new FakeKeyStorageClient());
  }

  @Provides
  public CloudAeadSelector provideAeadSelector() {
    return FakeDataKeyUtil.getAeadSelector();
  }

  @Override
  public void configure() {
    install(new InMemoryTestEnv());
    bind(String.class).annotatedWith(KeyEncryptionKeyUri.class).toInstance("fake-kms://fake-id");
    bind(KeyStorageClient.class).to(FakeKeyStorageClient.class);
  }
}
