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

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import static com.google.scp.coordinator.keymanagement.keystorage.service.gcp.testing.KeyStorageHttpFunctionModule.KMS_ENDPOINT_ENV_VAR_NAME;
import static com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer.startContainerAndConnectToSpannerWithEnvs;

import com.google.common.collect.ImmutableMap;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AeadConfig;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.inject.multibindings.ProvidesIntoOptional;
import com.google.kms.LocalKmsServerContainer;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.keymanagement.keystorage.service.gcp.testing.LocalKeyStorageServiceHttpFunction;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.KeyStorageCloudFunctionContainerWithKms;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.KeyStorageEnvironmentVariables;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.LocalKmsEnvironmentVariables;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.TestLocalKmsServerContainer;
import com.google.scp.coordinator.keymanagement.testutils.gcp.GcpKeyManagementIntegrationTestEnv;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainer;
import com.google.scp.shared.util.KeysetHandleSerializerUtil;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Base64;
import java.util.Map;
import java.util.Optional;

/** Environment for running GCP key storage integration tests. */
public class GcpKeyStorageServiceTestEnvModule extends AbstractModule {

  @Provides
  @Singleton
  public KeysetHandle providesKeysetHandle() {
    try {
      AeadConfig.register();
      return KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM"));
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Failed to create keyset handle", e);
    }
  }

  /**
   * Provides the environment variable, which currently has just the encoded keyset handle string,
   * to the cloud function so that it can reconstruct the same keyset handle the test will use for
   * encryption/decryption.
   */
  @ProvidesIntoOptional(ProvidesIntoOptional.Type.ACTUAL)
  @Singleton
  @KeyStorageEnvironmentVariables
  public Optional<Map<String, String>> providesEnvVariables(KeysetHandle keysetHandle) {
    try {
      ByteString keysetHandleByteString =
          KeysetHandleSerializerUtil.toBinaryCleartext(keysetHandle);
      String encodedKeysetHandleString =
          Base64.getEncoder().encodeToString(keysetHandleByteString.toByteArray());
      return Optional.of(
          ImmutableMap.of(
              LocalKeyStorageServiceHttpFunction.KEYSET_HANDLE_ENCODE_STRING_ENV_NAME,
              encodedKeysetHandleString));
    } catch (IOException e) {
      throw new RuntimeException("Failed to create keyset handle encoded string", e);
    }
  }

  /**
   * Provides the environment variable, which is local kms server's endpoint, the test will use for
   * encryption/decryption.
   */
  @ProvidesIntoOptional(ProvidesIntoOptional.Type.ACTUAL)
  @Singleton
  @LocalKmsEnvironmentVariables
  public Optional<Map<String, String>> providesEnvVariables(
      @TestLocalKmsServerContainer LocalKmsServerContainer container) {
    return Optional.of(
        ImmutableMap.of(
            KMS_ENDPOINT_ENV_VAR_NAME,
            "http://"
                + container
                    .getContainerInfo()
                    .getNetworkSettings()
                    .getNetworks()
                    .entrySet()
                    .stream()
                    .findFirst()
                    .get()
                    .getValue()
                    .getIpAddress()
                + ":"
                + container.getHttpPort()));
  }

  /** Starts and provides a container for Key Storage Cloud Function Integration tests. */
  @Provides
  @Singleton
  @KeyStorageCloudFunctionContainerWithKms
  public CloudFunctionEmulatorContainer getKeyStorageFunctionContainerWithKms(
      SpannerEmulatorContainer spannerEmulatorContainer,
      @LocalKmsEnvironmentVariables Optional<Map<String, String>> envVariables) {
    return startContainerAndConnectToSpannerWithEnvs(
        spannerEmulatorContainer,
        envVariables,
        "LocalKeyStorageServiceHttpCloudFunction_deploy.jar",
        "javatests/com/google/scp/coordinator/keymanagement/keystorage/service/gcp/testing/",
        "com.google.scp.coordinator.keymanagement.keystorage.service.gcp.testing.LocalKeyStorageServiceHttpFunction");
  }

  @Override
  protected void configure() {
    install(new GcpKeyManagementIntegrationTestEnv());
  }
}
