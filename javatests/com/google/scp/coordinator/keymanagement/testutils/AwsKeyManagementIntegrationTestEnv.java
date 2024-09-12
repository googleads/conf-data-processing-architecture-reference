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

package com.google.scp.coordinator.keymanagement.testutils;

import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.KEY_LIMIT;

import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keyhosting.service.aws.testing.LocalGetEncryptionKeyHostingServiceModule;
import com.google.scp.coordinator.keymanagement.keyhosting.service.aws.testing.LocalPublicKeyHostingServiceModule;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestModule;

/**
 * Sets up LocalStack testing environment with:
 *
 * <ul>
 *   <li>PublicKeyHosting lambda
 *   <li>PrivateKeyHosting lambda (created lazily)
 *   <li>PrivateKeyHostingV1 lambda (created lazily)
 *   <li>DynamoDb client
 *   <li>A KeyDB table which is cleaned between test runs.
 */
public final class AwsKeyManagementIntegrationTestEnv extends AbstractModule {
  @Override
  public void configure() {
    install(new AwsIntegrationTestModule());
    install(new KeyDbIntegrationTestModule());
    bind(Integer.class).annotatedWith(KeyLimit.class).toInstance(KEY_LIMIT);
    install(new LocalGetEncryptionKeyHostingServiceModule());
    install(new LocalPublicKeyHostingServiceModule());
  }
}
