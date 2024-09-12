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

package com.google.scp.coordinator.keymanagement.testutils.gcp;

import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbTestModule;
import com.google.scp.coordinator.keymanagement.testutils.gcp.cloudfunction.KeyManagementCloudFunctionContainersModule;

/** Environment for running GCP key management integration tests. */
public final class GcpKeyManagementIntegrationTestEnv extends AbstractModule {

  @Override
  protected void configure() {
    install(new SpannerKeyDbTestModule());
    install(new KeyManagementCloudFunctionContainersModule());
  }
}
