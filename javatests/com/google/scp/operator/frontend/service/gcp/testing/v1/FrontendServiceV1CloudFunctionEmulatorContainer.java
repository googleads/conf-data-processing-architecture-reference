/*
 * Copyright 2023 Google LLC
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

package com.google.scp.operator.frontend.service.gcp.testing.v1;

import static com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer.startContainerAndConnectToSpannerAndPubSub;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import com.google.scp.shared.testutils.gcp.PubSubEmulatorContainer;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainer;
import java.util.Optional;

/** Cloud function emulator container for frontend service integration test */
public class FrontendServiceV1CloudFunctionEmulatorContainer extends AbstractModule {
  public FrontendServiceV1CloudFunctionEmulatorContainer() {}

  @Provides
  @Singleton
  public CloudFunctionEmulatorContainer getFunctionContainer(
      SpannerEmulatorContainer spannerEmulatorContainer,
      PubSubEmulatorContainer pubSubEmulatorContainer) {
    return startContainerAndConnectToSpannerAndPubSub(
        spannerEmulatorContainer,
        pubSubEmulatorContainer,
        Optional.empty(),
        "LocalFrontendServiceV1HttpCloudFunction_deploy.jar",
        "javatests/com/google/scp/operator/frontend/service/gcp/testing/v1/",
        "com.google.scp.operator.frontend.service.gcp.testing.v1.LocalFrontendServiceV1HttpFunction");
  }
}
