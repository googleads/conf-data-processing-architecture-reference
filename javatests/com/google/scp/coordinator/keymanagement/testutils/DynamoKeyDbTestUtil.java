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

import static com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb.KEY_ID;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.KeySplitDataAttributeConverter.KEY_SPLIT_KEY_ENCRYPTION_KEY_URI;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.KeySplitDataAttributeConverter.PUBLIC_KEY_SIGNATURE;
import static software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues.stringValue;
import static software.amazon.awssdk.regions.Region.US_WEST_2;

import com.google.common.collect.ImmutableMap;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import java.time.Instant;
import java.util.stream.IntStream;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.AttributeDefinition;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;
import software.amazon.awssdk.services.dynamodb.model.CreateTableRequest;
import software.amazon.awssdk.services.dynamodb.model.DeleteTableRequest;
import software.amazon.awssdk.services.dynamodb.model.KeySchemaElement;
import software.amazon.awssdk.services.dynamodb.model.KeyType;
import software.amazon.awssdk.services.dynamodb.model.ProvisionedThroughput;
import software.amazon.awssdk.services.dynamodb.model.PutItemRequest;
import software.amazon.awssdk.services.dynamodb.model.ScalarAttributeType;
import software.amazon.awssdk.services.dynamodb.waiters.DynamoDbWaiter;

public final class DynamoKeyDbTestUtil {

  public static final String DYNAMO_DOCKER_NAME = "amazon/dynamodb-local:1.16.0";
  public static final String LOCALHOST = "http://localhost:";
  public static final String DYNAMO_KEY_TABLE_NAME = "KeyDbTest";
  public static final Region DYNAMO_TABLE_REGION = US_WEST_2;
  public static final Integer KEY_LIMIT = 5;

  private DynamoKeyDbTestUtil() {}

  /** Creates new DynamoDb table with default DYNAMO_KEY_TABLE_NAME */
  public static void setUpTable(DynamoDbClient dynamoDbClient, int keyItemCount) {
    setUpTable(dynamoDbClient, DYNAMO_KEY_TABLE_NAME, keyItemCount);
  }

  /**
   * Creates new DynamoDb table with KeyId as PrimaryKey. Inserts keyItemCount-number of items.
   * Inserted items contain random data for each attribute defined in the DynamoKeyDbRecord schema
   */
  public static void setUpTable(DynamoDbClient dynamoDbClient, String tableName, int keyItemCount) {
    CreateTableRequest createTableRequest =
        CreateTableRequest.builder()
            .attributeDefinitions(
                AttributeDefinition.builder()
                    .attributeName(KEY_ID)
                    .attributeType(ScalarAttributeType.S)
                    .build())
            .keySchema(
                KeySchemaElement.builder().attributeName(KEY_ID).keyType(KeyType.HASH).build())
            .provisionedThroughput(
                ProvisionedThroughput.builder()
                    .readCapacityUnits(10L)
                    .writeCapacityUnits(10L)
                    .build())
            .tableName(tableName)
            .build();
    dynamoDbClient.createTable(createTableRequest);
    DynamoDbWaiter waiter = dynamoDbClient.waiter();
    waiter.waitUntilTableExists(r -> r.tableName(tableName));

    IntStream.range(0, keyItemCount).forEach(unused -> putItemRandomValues(dynamoDbClient));
  }

  /** Delete given DynamoDb table. Useful for cleaning dynamoDb after unit tests */
  public static void deleteTable(DynamoDbClient dynamoDbClient, String tableName) {
    dynamoDbClient.deleteTable(DeleteTableRequest.builder().tableName(tableName).build());
  }

  /** Puts one Key item with random values to dynamoDbClient */
  public static void putItemRandomValues(DynamoDbClient client) {
    putItem(client, FakeEncryptionKey.create());
  }

  /** Inserts a random key with the specified expiration time into KeyDb. */
  public static void putItemWithExpirationTime(DynamoDbClient client, Instant expirationTime) {
    putItem(client, FakeEncryptionKey.withExpirationTime(expirationTime));
  }

  /** Inserts the provided EncrpytionKey into DynamoDB */
  public static void putItem(DynamoDbClient dynamoDbClient, EncryptionKey encryptionKey) {
    PutItemRequest putItemRequest =
        PutItemRequest.builder()
            .tableName(DYNAMO_KEY_TABLE_NAME)
            .item(encryptionKeyAttributeMap(encryptionKey))
            .build();
    dynamoDbClient.putItem(putItemRequest);
  }

  /** Inserts the provided Attribute map into DynamoDB */
  public static void putItem(
      DynamoDbClient dynamoDbClient, ImmutableMap<String, AttributeValue> itemValues) {
    PutItemRequest putItemRequest =
        PutItemRequest.builder().tableName(DYNAMO_KEY_TABLE_NAME).item(itemValues).build();
    dynamoDbClient.putItem(putItemRequest);
  }

  /** Expected attribute map for the dynamodb schema of EncryptionKey */
  public static ImmutableMap<String, AttributeValue> encryptionKeyAttributeMap(
      EncryptionKey encryptionKey) {
    return ImmutableMap.copyOf(DynamoKeyDb.getDynamoDbTableSchema().itemToMap(encryptionKey, true));
  }

  /** Expected attribute map for the dynamodb eschema of KeySplitData */
  private static ImmutableMap<String, AttributeValue> keySplitDataAttributeMap(
      KeySplitData keySplitData) {
    return new ImmutableMap.Builder<String, AttributeValue>()
        .put(
            KEY_SPLIT_KEY_ENCRYPTION_KEY_URI,
            stringValue(keySplitData.getKeySplitKeyEncryptionKeyUri()))
        .put(PUBLIC_KEY_SIGNATURE, stringValue(keySplitData.getPublicKeySignature()))
        .build();
  }
}
