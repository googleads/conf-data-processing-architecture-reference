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

import static com.google.common.truth.Truth.assertThat;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.Range;
import com.google.crypto.tink.CleartextKeysetHandle;
import com.google.crypto.tink.JsonKeysetReader;
import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing.KeyGenerationLambdaStarter;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.KeyDbIntegrationTestModule;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestModule;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import com.google.scp.shared.util.PublicKeyConversionUtil;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Map;
import javax.inject.Inject;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.testcontainers.containers.Network;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;
import software.amazon.awssdk.services.dynamodb.model.ScanRequest;
import software.amazon.awssdk.services.kms.KmsClient;

/**
 * Tests that the end-to-end key rotation process works correctly. Uses localstack version of AWS
 * KMS and DynamoDB.
 */
@RunWith(JUnit4.class)
public final class KeyGenerationLambdaIntegrationTest {

  private static final Integer KEY_COUNT = 3;
  private static final Integer VALIDITY_DAYS = 5;
  private static final Integer TTL_DAYS = 30;

  @Rule public final Acai acai = new Acai(TestEnv.class);

  @Inject LocalStackContainer localstack;
  @Inject DynamoDbClient ddbClient;
  @Inject KmsClient kmsClient;
  @Inject Network network;
  @Inject @DynamoKeyDbTableName String tableName;
  @Inject DynamoKeyDb keyDb;

  private String keyArn;

  @Before
  public void setUp() throws Exception {
    var keyId = AwsIntegrationTestUtil.createKmsKey(kmsClient);
    keyArn = String.format("arn:aws:kms:us-east-1:000000000000:key/%s", keyId);
  }

  /** Returns all items currently in the dynamodb table. */
  private List<Map<String, AttributeValue>> getItems() {
    var scanRequest = ScanRequest.builder().tableName(tableName).build();
    var scanResponse = ddbClient.scan(scanRequest);
    return scanResponse.items();
  }

  /** Returns a map of environment variables necessary to start the lambda. */
  private Map<String, String> getEnv(String tableName, String keyArn) {
    return ImmutableMap.<String, String>builder()
        .put("KEYSTORE_TABLE_REGION", "us-east-1")
        .put("KEYSTORE_ENDPOINT_OVERRIDE", AwsIntegrationTestModule.getInternalEndpoint())
        .put("AWS_ACCESS_KEY_ID", localstack.getAccessKey())
        .put("AWS_SECRET_ACCESS_KEY", localstack.getSecretKey())
        .put("KMS_ENDPOINT_OVERRIDE", AwsIntegrationTestModule.getInternalEndpoint())
        .put("ENCRYPTION_KEY_ARN", keyArn)
        .put("KEYSTORE_TABLE_NAME", tableName)
        .put("KEY_GENERATION_KEY_COUNT", KEY_COUNT.toString())
        .put("KEY_GENERATION_VALIDITY_IN_DAYS", VALIDITY_DAYS.toString())
        .put("KEY_GENERATION_TTL_IN_DAYS", TTL_DAYS.toString())
        .build();
  }

  @Test
  public void insert_item_success() throws Exception {
    var env = getEnv(tableName, keyArn);
    assertThat(getItems().size()).isEqualTo(0);

    KeyGenerationLambdaStarter.runKeyRotationLambda(env, network);

    assertThat(getItems().size()).isEqualTo(KEY_COUNT);
    var item = getItems().get(0);
    assertThat(item.get("KeyEncryptionKeyUri").s()).isEqualTo("aws-kms://" + keyArn);
    assertThat(item.get("PrivateKey").s())
        .contains("type.googleapis.com/google.crypto.tink.HpkePrivateKey");
    assertThat(item.get("PublicKey").s())
        .contains("type.googleapis.com/google.crypto.tink.HpkePublicKey");
    long validDaysInMillis = Instant.now().plus(VALIDITY_DAYS, ChronoUnit.DAYS).toEpochMilli();
    // Expiration time is within 10 seconds of validDays to account for delay in test
    assertThat(Long.valueOf(item.get("ExpirationTime").n()))
        .isIn(Range.closed(validDaysInMillis - 10000, validDaysInMillis));
    long ttlDaysSec = Instant.now().plus(TTL_DAYS, ChronoUnit.DAYS).getEpochSecond();
    // TTL is within 10 seconds of ttlDays to account for delay in test
    assertThat(Long.valueOf(item.get("TtlTime").n()))
        .isIn(Range.closed(ttlDaysSec - 10, ttlDaysSec));
  }

  @Test
  public void insert_item_invalid_table() throws Exception {
    var env = getEnv("invalid", keyArn);
    assertThat(getItems().size()).isEqualTo(0);

    KeyGenerationLambdaStarter.runKeyRotationLambda(env, network);

    assertThat(getItems().size()).isEqualTo(0);
  }

  @Test
  public void insert_item_invalid_key() throws Exception {
    var env = getEnv(tableName, "invalid");
    assertThat(getItems().size()).isEqualTo(0);

    KeyGenerationLambdaStarter.runKeyRotationLambda(env, network);

    assertThat(getItems().size()).isEqualTo(0);
  }

  @Test
  public void insert_item_public_key() throws Exception {
    var env = getEnv(tableName, keyArn);
    assertThat(getItems().size()).isEqualTo(0);

    KeyGenerationLambdaStarter.runKeyRotationLambda(env, network);

    var items = getItems();
    assertThat(items.size()).isEqualTo(KEY_COUNT);
    var item = items.get(0);
    var publicKeyMaterial = item.get("PublicKeyMaterial").s();
    var publicKeysetHandle = item.get("PublicKey").s();
    // Verify that the PublicKeyMaterial field is equivalent to the raw public key extracted from
    // the KeysetHandle in the PublicKey field.
    var rawPublicKey =
        PublicKeyConversionUtil.getPublicKey(
            CleartextKeysetHandle.read(JsonKeysetReader.withString(publicKeysetHandle)));

    assertThat(publicKeyMaterial).isEqualTo(rawPublicKey);
  }

  @Test
  public void doesNotUpdateFreshKeys() throws Exception {
    var env = getEnv(tableName, keyArn);
    for (var i = 0; i < 5; i++) {
      // Create 5 keys that expire in 5 days (outside the key refresh window).
      keyDb.createKey(FakeEncryptionKey.withExpirationTime(Instant.now().plus(Duration.ofDays(5))));
    }
    assertThat(getItems().size()).isEqualTo(5);

    KeyGenerationLambdaStarter.runKeyRotationLambda(env, network);

    // Ensure only one new set of keys were created.
    assertThat(getItems().size()).isEqualTo(5);
  }

  @Test
  public void updatesExpiringKeys() throws Exception {
    var env = getEnv(tableName, keyArn);
    for (var i = 0; i < 5; i++) {
      // Create 5 keys that expire within the key refresh window.
      keyDb.createKey(FakeEncryptionKey.withExpirationTime(Instant.now().plusSeconds(100)));
    }
    assertThat(getItems().size()).isEqualTo(5);

    KeyGenerationLambdaStarter.runKeyRotationLambda(env, network);

    assertThat(getItems().size()).isEqualTo(8 /* 5 + KEY_COUNT */);
  }

  private static class TestEnv extends AbstractModule {
    @Override
    public void configure() {
      var network = Network.newNetwork();
      install(new AwsIntegrationTestModule(network));
      install(new KeyDbIntegrationTestModule());
      bind(Network.class).toInstance(network);
    }
  }
}
