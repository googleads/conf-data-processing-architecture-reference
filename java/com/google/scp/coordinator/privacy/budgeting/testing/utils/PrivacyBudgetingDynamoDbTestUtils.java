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

package com.google.scp.coordinator.privacy.budgeting.testing.utils;

import static com.google.scp.coordinator.privacy.budgeting.dao.aws.model.AwsPrivacyBudgetRecord.PRIVACY_BUDGET_RECORD_KEY;
import static com.google.scp.coordinator.privacy.budgeting.dao.aws.model.AwsPrivacyBudgetRecord.REPORTING_WINDOW;
import static software.amazon.awssdk.services.dynamodb.model.BillingMode.PAY_PER_REQUEST;

import com.google.common.collect.ImmutableMap;
import software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.AttributeDefinition;
import software.amazon.awssdk.services.dynamodb.model.CreateTableRequest;
import software.amazon.awssdk.services.dynamodb.model.DeleteTableRequest;
import software.amazon.awssdk.services.dynamodb.model.KeySchemaElement;
import software.amazon.awssdk.services.dynamodb.model.KeyType;
import software.amazon.awssdk.services.dynamodb.model.ProvisionedThroughput;
import software.amazon.awssdk.services.dynamodb.model.PutItemRequest;
import software.amazon.awssdk.services.dynamodb.model.ScalarAttributeType;
import software.amazon.awssdk.services.dynamodb.waiters.DynamoDbWaiter;

/**
 * Util class that contains common methods for setting up DynamoDb for privacy budgeting service.
 */
public final class PrivacyBudgetingDynamoDbTestUtils {

  public static final String DYNAMODB_DOCKER_IMAGE = "amazon/dynamodb-local:1.16.0";
  public static final String PRIVACY_BUDGET_TABLE_NAME = "PrivacyBudgetDatabase";
  public static final String IDENTITY_TABLE_NAME = "IdentityDb";

  /**
   * Creates the privacy budget table and waits until the table is created before returning.
   *
   * @param ddb The dynamodb client to use.
   */
  public static void createPrivacyBudgetTable(DynamoDbClient ddb) {
    CreateTableRequest request =
        CreateTableRequest.builder()
            .attributeDefinitions(
                AttributeDefinition.builder()
                    .attributeName(PRIVACY_BUDGET_RECORD_KEY)
                    .attributeType(ScalarAttributeType.S)
                    .build(),
                AttributeDefinition.builder()
                    .attributeName(REPORTING_WINDOW)
                    .attributeType(ScalarAttributeType.N)
                    .build())
            .keySchema(
                KeySchemaElement.builder()
                    .attributeName(PRIVACY_BUDGET_RECORD_KEY)
                    .keyType(KeyType.HASH)
                    .build(),
                KeySchemaElement.builder()
                    .attributeName(REPORTING_WINDOW)
                    .keyType(KeyType.RANGE)
                    .build())
            .tableName(PRIVACY_BUDGET_TABLE_NAME)
            .billingMode(PAY_PER_REQUEST)
            .build();
    ddb.createTable(request);
    DynamoDbWaiter waiter = ddb.waiter();
    waiter.waitUntilTableExists(r -> r.tableName(PRIVACY_BUDGET_TABLE_NAME));
  }

  /**
   * Deletes the privacy budget table and waits for table to be deleted before returning.
   *
   * @param ddb The dynamodb client to use.
   */
  public static void deletePrivacyBudgetTable(DynamoDbClient ddb) {
    ddb.deleteTable(DeleteTableRequest.builder().tableName(PRIVACY_BUDGET_TABLE_NAME).build());
    DynamoDbWaiter waiter = ddb.waiter();
    waiter.waitUntilTableNotExists(r -> r.tableName(PRIVACY_BUDGET_TABLE_NAME));
  }

  public static void createIdentityDbTable(DynamoDbClient ddb) {
    CreateTableRequest request =
        CreateTableRequest.builder()
            .attributeDefinitions(
                AttributeDefinition.builder()
                    .attributeName("IamRole")
                    .attributeType(ScalarAttributeType.S)
                    .build())
            .keySchema(
                KeySchemaElement.builder().attributeName("IamRole").keyType(KeyType.HASH).build())
            .provisionedThroughput(
                ProvisionedThroughput.builder()
                    .readCapacityUnits(10L)
                    .writeCapacityUnits(10L)
                    .build())
            .tableName(IDENTITY_TABLE_NAME)
            .build();
    ddb.createTable(request);
    DynamoDbWaiter waiter = ddb.waiter();
    waiter.waitUntilTableExists(r -> r.tableName(IDENTITY_TABLE_NAME));
  }

  public static void insertIdentityDbItem(
      DynamoDbClient ddb, String iamRole, String attributionReportTo) {
    ddb.putItem(
        PutItemRequest.builder()
            .tableName(IDENTITY_TABLE_NAME)
            .item(
                ImmutableMap.of(
                    "IamRole",
                    AttributeValues.stringValue(iamRole),
                    "ReportingOrigin",
                    AttributeValues.stringValue(attributionReportTo)))
            .build());
  }
}
