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

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb.KEY_SPLIT_DATA;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb.KEY_TYPE;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb.STATUS;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb.TTL_TIME;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.DYNAMO_KEY_TABLE_NAME;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.KEY_LIMIT;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.encryptionKeyAttributeMap;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.putItem;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.putItemRandomValues;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.putItemWithExpirationTime;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.setUpTable;
import static com.google.scp.shared.api.model.Code.ALREADY_EXISTS;
import static java.time.Instant.now;
import static java.time.temporal.ChronoUnit.DAYS;
import static java.util.UUID.randomUUID;
import static org.junit.Assert.assertThrows;
import static software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues.stringValue;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb.InvalidEncryptionKeyStatusException;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDbBaseTest;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyStatusProto.EncryptionKeyStatus;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.util.HashMap;
import java.util.List;
import java.util.stream.IntStream;
import java.util.stream.Stream;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;
import software.amazon.awssdk.services.dynamodb.model.DeleteTableRequest;

@RunWith(JUnit4.class)
public class DynamoKeyDbTest extends KeyDbBaseTest {

  @Rule public final Acai acai = new Acai(DynamoKeyDbTestEnv.class);
  @Inject private DynamoKeyDb dynamoKeyDb;
  @Inject private DynamoDbClient dynamoDbClient;

  @Before
  public void setUp() {
    setUpTable(dynamoDbClient, 0);
  }

  @After
  public void deleteTable() {
    dynamoDbClient.deleteTable(
        DeleteTableRequest.builder().tableName(DYNAMO_KEY_TABLE_NAME).build());
  }

  @Test
  public void getActiveKeys_atLimitCount() throws ServiceException {
    IntStream.range(0, 10).forEach(unused -> putItemRandomValues(dynamoDbClient));

    ImmutableList<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isNotEmpty();
    assertThat(keys).hasSize(KEY_LIMIT);
  }

  @Test
  public void getActiveKeys_whenGreaterThanLimitCount() throws ServiceException {
    IntStream.range(0, 13).forEach(unused -> putItemRandomValues(dynamoDbClient));

    ImmutableList<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isNotEmpty();
    assertThat(keys).hasSize(KEY_LIMIT);
  }

  @Test
  public void getActiveKeys_whenLessThanLimitCount() throws ServiceException {
    IntStream.range(0, 3).forEach(unused -> putItemRandomValues(dynamoDbClient));

    ImmutableList<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isNotEmpty();
    assertThat(keys).hasSize(3);
  }

  @Test
  public void getActiveKeys_whenEmpty() throws ServiceException {
    ImmutableList<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isEmpty();
  }

  @Test
  public void getActiveKeys_filterNonactiveKeys() throws ServiceException {
    EncryptionKey testKey =
        FakeEncryptionKey.create().toBuilder().setStatus(EncryptionKeyStatus.INACTIVE).build();
    putItem(dynamoDbClient, testKey);
    List<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isEmpty();
  }

  @Test
  public void getActiveKeys_expiredKeysAreFiltered() throws ServiceException {
    putItemWithExpirationTime(dynamoDbClient, now().minus(7, DAYS));

    List<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isEmpty();
  }

  @Test
  public void getActiveKeys_keysAreSorted() throws ServiceException {
    // Insert items unsorted.
    for (int i : ImmutableList.of(7, 5, 4, 2, 1, 5, 9, 8, 10, 3)) {
      putItemWithExpirationTime(dynamoDbClient, now().plus(i + 1, DAYS));
    }

    List<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(10);

    assertThat(keys).isNotEmpty();
    assertThat(keys).hasSize(10);
    for (int i = 1; i < 10; i++) {
      // Descending order
      assertThat(keys.get(i - 1).getExpirationTime())
          .isGreaterThan(keys.get(i).getExpirationTime());
    }
  }

  @Test
  public void getAllKeys_success() throws ServiceException {
    IntStream.range(0, 100).forEach(unused -> putItemRandomValues(dynamoDbClient));

    List<EncryptionKey> keys = dynamoKeyDb.getAllKeys();

    assertThat(keys).isNotEmpty();
    assertThat(keys).hasSize(100);
  }

  /** Full roundtrip test */
  @Test
  public void getKey_returnsKey() throws Exception {
    EncryptionKey expectedKey = FakeEncryptionKey.create();
    putItem(dynamoDbClient, expectedKey);

    EncryptionKey receivedKey = dynamoKeyDb.getKey(expectedKey.getKeyId());

    assertThat(receivedKey).isEqualTo(expectedKey);
  }

  /** Ensure that getKey expects the correct attribute map schema */
  @Test
  public void getKey_attributeMap() throws Exception {
    EncryptionKey expectedKey = FakeEncryptionKey.create();
    ImmutableMap<String, AttributeValue> attributeMap = encryptionKeyAttributeMap(expectedKey);
    putItem(dynamoDbClient, attributeMap);

    EncryptionKey receivedKey = dynamoKeyDb.getKey(expectedKey.getKeyId());

    assertThat(receivedKey).isEqualTo(expectedKey);
  }

  /** Ensure that the missing fields (with default values) get loaded properly */
  @Test
  public void getKey_populatesDefaultValues() throws Exception {
    EncryptionKey expectedKey = FakeEncryptionKey.create();
    HashMap<String, AttributeValue> attributeMap =
        new HashMap<>(encryptionKeyAttributeMap(expectedKey));
    // Remove all fields with default values
    attributeMap.remove(STATUS);
    attributeMap.remove(TTL_TIME);
    attributeMap.remove(KEY_SPLIT_DATA);
    attributeMap.remove(KEY_TYPE);

    putItem(dynamoDbClient, ImmutableMap.copyOf(attributeMap));

    EncryptionKey receivedKey = dynamoKeyDb.getKey(expectedKey.getKeyId());

    // Fields without default values
    assertThat(receivedKey.getKeyId()).isEqualTo(expectedKey.getKeyId());
    assertThat(receivedKey.getPublicKey()).isEqualTo(expectedKey.getPublicKey());
    assertThat(receivedKey.getPublicKeyMaterial()).isEqualTo(expectedKey.getPublicKeyMaterial());
    assertThat(receivedKey.getJsonEncodedKeyset()).isEqualTo(expectedKey.getJsonEncodedKeyset());
    assertThat(receivedKey.getKeyEncryptionKeyUri())
        .isEqualTo(expectedKey.getKeyEncryptionKeyUri());
    assertThat(receivedKey.getCreationTime()).isEqualTo(expectedKey.getCreationTime());
    assertThat(receivedKey.getExpirationTime()).isEqualTo(expectedKey.getExpirationTime());

    // Fields with default values
    assertThat(receivedKey.getStatus()).isEqualTo(EncryptionKeyStatus.ACTIVE);
    assertThat(receivedKey.getTtlTime()).isEqualTo(0L);
    assertThat(receivedKey.getKeySplitDataList()).isEqualTo(ImmutableList.of());
    assertThat(receivedKey.getKeyType()).isEqualTo("");
  }

  @Test
  public void getKey_returnsNotFound() {
    ServiceException exception =
        assertThrows(ServiceException.class, () -> dynamoKeyDb.getKey("unset"));

    assertThat(exception.getErrorCode()).isEqualTo(Code.NOT_FOUND);
  }

  /** Tests that a Service Exception wraps an upstream dynamodb error */
  @Test
  public void getKey_returnsInternalError() {
    // Attempting to query a blank key is known to trigger a dynamodb exception when calling
    // getItem.
    ServiceException exception = assertThrows(ServiceException.class, () -> dynamoKeyDb.getKey(""));

    assertThat(exception.getErrorCode()).isEqualTo(Code.INTERNAL);
  }

  @Test
  public void getKey_mismatchStatus() {
    EncryptionKey expectedKey = FakeEncryptionKey.create();
    HashMap<String, AttributeValue> attributeMap =
        new HashMap<>(encryptionKeyAttributeMap(expectedKey));
    attributeMap.put(STATUS, stringValue("INVALID"));
    putItem(dynamoDbClient, ImmutableMap.copyOf(attributeMap));

    assertThrows(
        InvalidEncryptionKeyStatusException.class,
        () -> dynamoKeyDb.getKey(expectedKey.getKeyId()));
  }

  @Test
  public void createKey_successPrivateKey() throws Exception {
    String keyId = "test-key-id";
    EncryptionKey expectedKey =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("")
            .setJsonEncodedKeyset("12345")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    dynamoKeyDb.createKey(expectedKey);
    EncryptionKey receivedKey = dynamoKeyDb.getKey(keyId);

    assertThat(receivedKey.getJsonEncodedKeyset()).isEqualTo(expectedKey.getJsonEncodedKeyset());
  }

  @Test
  public void createKey_successPublicKey() throws Exception {
    String keyId = "asdf";
    long creationTime = now().toEpochMilli();
    long expirationTime = now().plus(7, DAYS).toEpochMilli();
    EncryptionKey expectedKey =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("12345")
            .setJsonEncodedKeyset("")
            .setCreationTime(creationTime)
            .setExpirationTime(expirationTime)
            .build();

    dynamoKeyDb.createKey(expectedKey);
    ImmutableList<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isNotEmpty();
    assertThat(keys).hasSize(1);
    assertThat(keys.get(0).getPublicKey()).isEqualTo(expectedKey.getPublicKey());
  }

  @Test
  public void createKey_successWithDefaults() throws Exception {
    EncryptionKey expectedKey =
        EncryptionKey.newBuilder()
            .setKeyId(randomUUID().toString())
            .setPublicKey(randomUUID().toString())
            .setJsonEncodedKeyset("")
            .setCreationTime(now().toEpochMilli())
            .setExpirationTime(now().plus(7, DAYS).toEpochMilli())
            .build();

    dynamoKeyDb.createKey(expectedKey);
    ImmutableList<EncryptionKey> keys = dynamoKeyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isNotEmpty();
    assertThat(keys).hasSize(1);

    EncryptionKey key = keys.get(0);
    // EncryptionKeyStatus default is ACTIVE
    assertThat(key.getStatus()).isEqualTo(EncryptionKeyStatus.ACTIVE);

    // TtlTime default is 0
    assertThat(key.getTtlTime()).isEqualTo(0L);
  }

  @Test
  public void createKey_alreadyExists() throws Exception {
    String keyId = "test-key-id";
    String privateKey = "{test-key-text}";
    String privateKey2 = "{test-2-key-text}";
    EncryptionKey key1 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("")
            .setJsonEncodedKeyset(privateKey)
            .setStatus(EncryptionKeyStatus.ACTIVE)
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();
    EncryptionKey key2 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("")
            .setJsonEncodedKeyset(privateKey2)
            .setStatus(EncryptionKeyStatus.INACTIVE)
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    dynamoKeyDb.createKey(key1);
    try {
      dynamoKeyDb.createKey(key2, false);
    } catch (ServiceException e) {
      assertThat(e.getErrorCode()).isEqualTo(ALREADY_EXISTS);
    }
  }

  @Test
  public void createKeys_batchInsert() throws Exception {
    var keys =
        Stream.of("test-key-id-1", "test-key-id-2", "test-key-id-3")
            .map(
                keyId ->
                    EncryptionKey.newBuilder()
                        .setKeyId(keyId)
                        .setPublicKey("foo")
                        .setJsonEncodedKeyset("{foo}")
                        .setCreationTime(0L)
                        .setExpirationTime(0L)
                        .build())
            .collect(toImmutableList());

    dynamoKeyDb.createKeys(keys);

    var items = dynamoKeyDb.getAllKeys();
    assertThat(items).hasSize(3);
    var keyIds = items.stream().map(EncryptionKey::getKeyId).sorted().collect(toImmutableList());
    assertThat(keyIds)
        .isEqualTo(ImmutableList.of("test-key-id-1", "test-key-id-2", "test-key-id-3"));
  }

  @Test
  public void schemaIncludesKeyMaterialAndArn() throws Exception {
    var keyId = "foo";
    var publicKeyMaterial = "public key material";
    var encryptionKeyUri = "aws-kms://arn::encryptionkeyarn";

    var key =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("foo")
            .setPublicKeyMaterial(publicKeyMaterial)
            .setJsonEncodedKeyset("{foo}")
            .setKeyEncryptionKeyUri(encryptionKeyUri)
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    dynamoKeyDb.createKey(key);
    var returnedKey = dynamoKeyDb.getKey(keyId);

    assertThat(returnedKey.getPublicKeyMaterial()).isEqualTo(publicKeyMaterial);
    assertThat(returnedKey.getKeyEncryptionKeyUri()).isEqualTo(encryptionKeyUri);
  }
}
