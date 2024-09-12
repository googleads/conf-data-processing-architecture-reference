/*
 * Copyright 2024 Google LLC
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

package com.google.scp.coordinator.keymanagement.keyhosting.service.common;

import com.google.common.collect.ImmutableList;
import com.google.inject.AbstractModule;
import com.google.inject.multibindings.ProvidesIntoMap;
import com.google.inject.multibindings.StringMapKey;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.v1.GetActivePublicKeysTask;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ApiTask;
import java.util.List;

/* Guice module that configures the base URL and the API tasks for the Public Key Service. */
public class PublicKeyServiceModule extends AbstractModule {

  @ProvidesIntoMap
  @StringMapKey("/v1alpha")
  List<ApiTask> provideV1AlphaTasks(
      com.google.scp.coordinator.keymanagement.keyhosting.tasks.GetActivePublicKeysTask
          getActivePublicKeysTask) {
    return ImmutableList.of(getActivePublicKeysTask);
  }

  @ProvidesIntoMap
  @StringMapKey("/v1beta")
  List<ApiTask> provideV1AlphaTasks(GetActivePublicKeysTask getActivePublicKeysTask) {
    return ImmutableList.of(getActivePublicKeysTask);
  }
}
