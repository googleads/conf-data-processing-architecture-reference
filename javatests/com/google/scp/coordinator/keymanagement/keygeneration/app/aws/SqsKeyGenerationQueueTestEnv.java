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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationQueueMessageLeaseSeconds;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsMaxWaitTimeSeconds;

import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.testutils.aws.TestKeyGenerationQueueModule;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestModule;

public final class SqsKeyGenerationQueueTestEnv extends AbstractModule {
  @Override
  public void configure() {
    install(new AwsIntegrationTestModule());
    install(new TestKeyGenerationQueueModule());
    bind(int.class).annotatedWith(KeyGenerationSqsMaxWaitTimeSeconds.class).toInstance(2);
    bind(int.class)
        .annotatedWith(KeyGenerationQueueMessageLeaseSeconds.class)
        .toInstance(60 * 60 * 2); // two hours
  }
}
