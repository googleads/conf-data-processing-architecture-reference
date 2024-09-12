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

package com.google.scp.coordinator.keymanagement.shared.dao.aws;

import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.DYNAMO_KEY_TABLE_NAME;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.KEY_LIMIT;

import com.google.acai.TestScoped;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.testutils.aws.DynamoDbIntegrationTestModule;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;

/** Abstract module for tests using local DynamoDB instance */
public class DynamoKeyDbTestEnv extends AbstractModule {

  @Provides
  @TestScoped
  @KeyDbClient
  public DynamoDbEnhancedClient getDynamoDbEnhancedClient(DynamoDbClient dynamoDbClient) {
    return DynamoDbEnhancedClient.builder().dynamoDbClient(dynamoDbClient).build();
  }

  @Override
  public void configure() {
    install(new DynamoDbIntegrationTestModule());
    bind(KeyDb.class).to(DynamoKeyDb.class);
    bind(String.class).annotatedWith(DynamoKeyDbTableName.class).toInstance(DYNAMO_KEY_TABLE_NAME);
    bind(Integer.class).annotatedWith(KeyLimit.class).toInstance(KEY_LIMIT);
  }
}
