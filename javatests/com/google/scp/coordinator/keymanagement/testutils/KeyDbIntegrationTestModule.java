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

import com.google.acai.BeforeTest;
import com.google.acai.TestingService;
import com.google.acai.TestingServiceModule;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import java.util.UUID;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.DeleteTableRequest;

/**
 * Module which create a KeyDb using the provided DynamoDB client and clean it up after every test.
 *
 * <p>This is intended for use with a fake DynamoDB instance (e.g. via LocalStack) but could be used
 * with prod AWS as well.
 */
public final class KeyDbIntegrationTestModule extends AbstractModule {

  // Randomize name to allow for use against a shared resource (e.g. prod AWS DynamoDB).
  public static final String TABLE_NAME = "KeyDBIntegrationTest_" + UUID.randomUUID().toString();

  @Provides
  @KeyDbClient
  public DynamoDbEnhancedClient getDynamoDbEnhancedClient(DynamoDbClient dynamoDbClient) {
    return DynamoDbEnhancedClient.builder().dynamoDbClient(dynamoDbClient).build();
  }

  @Override
  public void configure() {
    install(TestingServiceModule.forServices(KeyDbCleanupService.class));
    bind(String.class).annotatedWith(DynamoKeyDbTableName.class).toInstance(TABLE_NAME);
  }

  public static class KeyDbCleanupService implements TestingService {
    @Inject @DynamoKeyDbTableName private String keyDbTableName;
    @Inject private DynamoDbClient ddbClient;
    private boolean tableExists = false;

    @BeforeTest
    void createTable() {
      try {
        // Attempt to delete the table in @BeforeTest (ignoring exceptions when it doesn't exist)
        // because @AfterTest isn't guaranteed to run, which can lead to leaked state
        ddbClient.deleteTable(DeleteTableRequest.builder().tableName(keyDbTableName).build());
      } catch (Exception e) {
      }
      AwsIntegrationTestUtil.createKeyDbTable(ddbClient, keyDbTableName);
    }
  }
}
