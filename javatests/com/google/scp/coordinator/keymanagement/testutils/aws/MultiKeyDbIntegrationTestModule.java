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

package com.google.scp.coordinator.keymanagement.testutils.aws;

import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAKeyDbTableName;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorBKeyDbTableName;

import com.google.acai.BeforeTest;
import com.google.acai.TestingService;
import com.google.acai.TestingServiceModule;
import com.google.common.collect.ImmutableList;
import com.google.inject.AbstractModule;
import com.google.inject.BindingAnnotation;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.UUID;
import java.util.stream.IntStream;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.DeleteTableRequest;

/**
 * Module which creates multiple KeyDbs using the provided DynamoDB client and automatically creates
 * a new table for each test.
 *
 * <p>For integration tests which must test multiple KeyDbs (i.e. cross-coordinator tests) -- tests
 * which only require a singular KeyDb should use {@link KeyDbIntegrationTestModule}.
 *
 * <p>Provides Two DynamoDb tables accessible via String annotated with {@link
 * CoordinatorAKeyDbTableName} and {@link CoordinatorBKeyDbTableName}. Two {@link DynamoKeyDb}'s
 * interfaces annotated respectively with those two annotations are also provided for convenience.
 */
public final class MultiKeyDbIntegrationTestModule extends AbstractModule {
  private final ImmutableList<String> tableNames;

  /** Creates a 2-table setup. */
  public MultiKeyDbIntegrationTestModule() {
    this.tableNames = createNTableNames(2);
  }

  @Provides
  @KeyDbClient
  public DynamoDbEnhancedClient getDynamoDbEnhancedClient(DynamoDbClient dynamoDbClient) {
    return DynamoDbEnhancedClient.builder().dynamoDbClient(dynamoDbClient).build();
  }

  @Provides
  @DynamoKeyDbTableNames
  public ImmutableList<String> provideTableNames() {
    return tableNames;
  }

  @Provides
  @Singleton
  @CoordinatorAKeyDbTableName
  public DynamoKeyDb provideKeyDbA(
      @KeyDbClient DynamoDbEnhancedClient keyDbClient,
      @CoordinatorAKeyDbTableName String keyDbTableName) {
    return new DynamoKeyDb(keyDbClient, keyDbTableName);
  }

  @Provides
  @Singleton
  @CoordinatorBKeyDbTableName
  public DynamoKeyDb provideKeyDbB(
      @KeyDbClient DynamoDbEnhancedClient keyDbClient,
      @CoordinatorBKeyDbTableName String keyDbTableName) {
    return new DynamoKeyDb(keyDbClient, keyDbTableName);
  }

  @Override
  public void configure() {
    install(TestingServiceModule.forServices(MultiKeyDbCleanupService.class));
    bind(String.class)
        .annotatedWith(CoordinatorAKeyDbTableName.class)
        .toInstance(tableNames.get(0));
    bind(String.class)
        .annotatedWith(CoordinatorBKeyDbTableName.class)
        .toInstance(tableNames.get(1));
  }

  /**
   * Internal annotation for passing all generated tables to MultiKeyDbCleanupService -- could be
   * made public when needing to scale past 2 coordinators.
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  private @interface DynamoKeyDbTableNames {}

  /** Creates N tableNames with randomized names to avoid collisions against shared resources. */
  private static ImmutableList<String> createNTableNames(int numTables) {
    return IntStream.range(0, numTables)
        .mapToObj(
            index ->
                String.format("KeyDBIntegrationTest_%d_%s", index, UUID.randomUUID().toString()))
        .collect(ImmutableList.toImmutableList());
  }

  public static class MultiKeyDbCleanupService implements TestingService {
    @Inject @DynamoKeyDbTableNames private ImmutableList<String> tableNames;
    @Inject private DynamoDbClient ddbClient;

    @BeforeTest
    private void createTable() {
      // Attempt to delete the tables in @BeforeTest (ignoring exceptions when they don't exist)
      // because @AfterTest isn't guaranteed to run, which can lead to leaked state
      for (var tableName : tableNames) {
        try {
          ddbClient.deleteTable(DeleteTableRequest.builder().tableName(tableName).build());
        } catch (Exception e) {
        }
      }
      for (var tableName : tableNames) {
        AwsIntegrationTestUtil.createKeyDbTable(ddbClient, tableName);
      }
    }
  }
}
