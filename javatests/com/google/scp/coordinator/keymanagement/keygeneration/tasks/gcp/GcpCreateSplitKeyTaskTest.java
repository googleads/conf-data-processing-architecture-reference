/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp;

import static org.mockito.Mockito.spy;

import com.google.acai.Acai;
import com.google.acai.TestScoped;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AesCtrHmacAeadKeyManager;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTaskBase;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.SplitKeyGenerationTestEnv;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdFactory;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.UuidKeyIdFactory;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.PeerCoordinatorKeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.PeerCoordinatorKmsKeyAead;
import java.security.GeneralSecurityException;
import org.junit.Rule;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Test the full GcpCreateSplitKeyTask using a faked KeyStorageClient and spied-on dependencies. */
@RunWith(JUnit4.class)
public final class GcpCreateSplitKeyTaskTest extends GcpCreateSplitKeyTaskTestBase {
  @Rule public final Acai acai = new Acai(TestEnv.class);

  private static class TestEnv extends AbstractModule {
    @Provides
    @TestScoped
    @PeerCoordinatorKmsKeyAead
    public Aead providePeerCoordinatorAead() throws GeneralSecurityException {
      return spy(
          KeysetHandle.generateNew(AesCtrHmacAeadKeyManager.aes256CtrHmacSha256Template())
              .getPrimitive(Aead.class));
    }

    @Override
    public void configure() {
      install(new SplitKeyGenerationTestEnv());
      bind(CreateSplitKeyTaskBase.class).to(GcpCreateSplitKeyTask.class);
      bind(String.class)
          .annotatedWith(PeerCoordinatorKeyEncryptionKeyUri.class)
          .toInstance("fake-kms://fake-id-b");
      bind(KeyIdFactory.class).toInstance(new UuidKeyIdFactory());
    }
  }
}
