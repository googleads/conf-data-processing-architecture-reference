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

package com.google.scp.coordinator.keymanagement.testutils.gcp.cloudfunction;

import static com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer.startContainerAndConnectToSpanner;
import static com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer.startContainerAndConnectToSpannerWithEnvs;

import com.google.inject.AbstractModule;
import com.google.inject.Key;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.inject.TypeLiteral;
import com.google.inject.multibindings.OptionalBinder;
import com.google.kms.LocalKmsServerContainer;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.EncryptionKeyCoordinatorAEnvironmentVariables;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.EncryptionKeyCoordinatorBEnvironmentVariables;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.EncryptionKeyServiceCloudFunctionContainer;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.EncryptionKeyServiceCoordinatorBCloudFunctionContainer;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.KeyStorageCloudFunctionContainer;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.KeyStorageEnvironmentVariables;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.PublicKeyCloudFunctionContainer;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.TestLocalKmsServerContainer;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainer;
import java.util.Map;
import java.util.Optional;

/** Module that defines Cloud Function containers needed for Key Management integration tests. */
public class KeyManagementCloudFunctionContainersModule extends AbstractModule {

  public KeyManagementCloudFunctionContainersModule() {}

  @Override
  public void configure() {
    OptionalBinder.newOptionalBinder(
            binder(),
            Key.get(
                new TypeLiteral<Optional<Map<String, String>>>() {},
                KeyStorageEnvironmentVariables.class))
        .setDefault()
        .toInstance(Optional.empty());

    // Pass in optional parameters via environment parameters; defaults are used if empty.
    OptionalBinder.newOptionalBinder(
            binder(),
            Key.get(
                new TypeLiteral<Optional<Map<String, String>>>() {},
                EncryptionKeyCoordinatorAEnvironmentVariables.class))
        .setDefault()
        .toInstance(Optional.empty());
    OptionalBinder.newOptionalBinder(
            binder(),
            Key.get(
                new TypeLiteral<Optional<Map<String, String>>>() {},
                EncryptionKeyCoordinatorBEnvironmentVariables.class))
        .setDefault()
        .toInstance(Optional.empty());
  }

  /** Starts and provides a container for Public Key Cloud Function Integration tests. */
  @Provides
  @Singleton
  @PublicKeyCloudFunctionContainer
  public CloudFunctionEmulatorContainer getFunctionContainer(
      SpannerEmulatorContainer spannerEmulatorContainer) {
    return startContainerAndConnectToSpanner(
        spannerEmulatorContainer,
        "LocalPublicKeyHttpCloudFunction_deploy.jar",
        "javatests/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp/testing/",
        "com.google.scp.coordinator.keymanagement.keyhosting.service.gcp.testing.LocalPublicKeyHttpFunction");
  }

  /**
   * Starts and provides a container for Encryption Key Cloud Function Integration tests. Note: This
   * is for Coordinator A.
   */
  @Provides
  @Singleton
  @EncryptionKeyServiceCloudFunctionContainer
  public CloudFunctionEmulatorContainer getEncryptionKeyFunctionContainer(
      SpannerEmulatorContainer spannerEmulatorContainer,
      @EncryptionKeyCoordinatorAEnvironmentVariables Optional<Map<String, String>> envVariables) {
    return startContainerAndConnectToSpannerWithEnvs(
        spannerEmulatorContainer,
        envVariables,
        "LocalEncryptionKeyServiceHttpCloudFunction_deploy.jar",
        "javatests/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp/testing/",
        "com.google.scp.coordinator.keymanagement.keyhosting.service.gcp.testing.LocalEncryptionKeyServiceHttpFunction");
  }

  /**
   * Starts and provides a container for Encryption Key Cloud Function Integration tests. Note: This
   * is for Coordinator B.
   */
  @Provides
  @Singleton
  @EncryptionKeyServiceCoordinatorBCloudFunctionContainer
  public CloudFunctionEmulatorContainer getEncryptionKeyCoordinatorBFunctionContainer(
      SpannerEmulatorContainer spannerEmulatorContainer,
      @EncryptionKeyCoordinatorBEnvironmentVariables Optional<Map<String, String>> envVariables) {
    return startContainerAndConnectToSpannerWithEnvs(
        spannerEmulatorContainer,
        envVariables,
        "LocalEncryptionKeyServiceHttpCloudFunction_deploy.jar",
        "javatests/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp/testing/",
        "com.google.scp.coordinator.keymanagement.keyhosting.service.gcp.testing.LocalEncryptionKeyServiceHttpFunction");
  }

  /** Starts and provides a container for Key Storage Cloud Function Integration tests. */
  @Provides
  @Singleton
  @KeyStorageCloudFunctionContainer
  public CloudFunctionEmulatorContainer getKeyStorageFunctionContainer(
      SpannerEmulatorContainer spannerEmulatorContainer,
      @KeyStorageEnvironmentVariables Optional<Map<String, String>> envVariables) {
    return startContainerAndConnectToSpannerWithEnvs(
        spannerEmulatorContainer,
        envVariables,
        "LocalKeyStorageServiceHttpCloudFunction_deploy.jar",
        "javatests/com/google/scp/coordinator/keymanagement/keystorage/service/gcp/testing/",
        "com.google.scp.coordinator.keymanagement.keystorage.service.gcp.testing.LocalKeyStorageServiceHttpFunction");
  }

  /* starts a container for local kms service, used by coordinator/keystorage/KeystorageServiceIntegrationTest*/
  @Provides
  @Singleton
  @TestLocalKmsServerContainer
  public LocalKmsServerContainer getLocalKmsServerContainer() {
    return LocalKmsServerContainer.startLocalKmsContainer(
        "LocalGcpKmsServer_deploy.jar",
        "javatests/com/google/kms/",
        "com.google.kms.LocalGcpKmsServer");
  }
}
