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

package com.google.scp.coordinator.keymanagement.shared.dao.gcp;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.UNSUPPORTED_OPERATION;
import static com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil.SPANNER_KEY_TABLE_NAME;
import static com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil.putItem;
import static com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil.putKeyWithActivationAndExpirationTimes;
import static com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil.putKeyWithExpiration;
import static com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil.putNItemsRandomValues;
import static com.google.scp.shared.api.model.Code.ALREADY_EXISTS;
import static org.awaitility.Awaitility.await;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.KeySet;
import com.google.cloud.spanner.Mutation;
import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDbBaseTest;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDbUtil;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.concurrent.TimeUnit;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class SpannerKeyDbTest extends KeyDbBaseTest {

  private static final int DEFAULT_KEY_ITEM_COUNT = 5;

  @Rule public final Acai acai = new Acai(SpannerKeyDbTestModule.class);
  @Inject @KeyDbClient private DatabaseClient dbClient;
  @Inject private SpannerKeyDb keyDb;

  @After
  public void deleteTable() {
    dbClient.write(ImmutableList.of(Mutation.delete(SPANNER_KEY_TABLE_NAME, KeySet.all())));
  }

  @Test
  public void createKey_successAdd() throws ServiceException {
    String keyId = "test-key-id";
    EncryptionKey expectedKey =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("12345")
            .setPublicKeyMaterial("12345")
            .setJsonEncodedKeyset("67890")
            .setKeyEncryptionKeyUri("URI")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    keyDb.createKey(expectedKey);
    EncryptionKey receivedKey = keyDb.getKey(keyId);

    assertThat(receivedKey.getKeyId()).isEqualTo(keyId);
    assertThat(receivedKey.getPublicKey()).isEqualTo(expectedKey.getPublicKey());
    assertThat(receivedKey.getJsonEncodedKeyset()).isEqualTo(expectedKey.getJsonEncodedKeyset());
  }

  @Test
  public void createKey_alreadyExists() throws ServiceException {
    String keyId = "test-key-id";
    EncryptionKey key1 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("012")
            .setPublicKeyMaterial("012")
            .setJsonEncodedKeyset("345")
            .setKeyEncryptionKeyUri("URI")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();
    EncryptionKey key2 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("678")
            .setPublicKeyMaterial("678")
            .setJsonEncodedKeyset("9ab")
            .setKeyEncryptionKeyUri("URI")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    keyDb.createKey(key1);
    try {
      keyDb.createKey(key2, false);
    } catch (ServiceException e) {
      assertThat(e.getErrorCode()).isEqualTo(ALREADY_EXISTS);
    }
  }

  @Test
  public void createKeys_success() throws ServiceException {
    String keyId = "1";
    EncryptionKey key =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("publicKey1")
            .setPublicKeyMaterial("material1")
            .setJsonEncodedKeyset("PrivateKey1")
            .setKeyEncryptionKeyUri("URI")
            .setExpirationTime(Instant.now().plus(Duration.ofSeconds(2000)).toEpochMilli())
            .setTtlTime(Instant.now().plus(Duration.ofDays(365)).getEpochSecond())
            .setActivationTime(Instant.now().plus(Duration.ofSeconds(1000)).toEpochMilli())
            .build();

    keyDb.createKeys(ImmutableList.of(key));
    EncryptionKey dbKey = keyDb.getKey(keyId);

    assertThat(dbKey.getKeyId()).isEqualTo(keyId);
    assertThat(dbKey.getPublicKey()).isEqualTo(key.getPublicKey());
    assertThat(dbKey.getJsonEncodedKeyset()).isEqualTo(key.getJsonEncodedKeyset());
  }

  @Test
  public void getKey_success() throws ServiceException {
    EncryptionKey expectedKey =
        FakeEncryptionKey.createBuilderWithDefaults()
            // SpannerKeyDb returns times based off seconds
            .setExpirationTime(Instant.now().plus(7, ChronoUnit.DAYS).toEpochMilli())
            .setActivationTime(Instant.now().toEpochMilli())
            .build();
    putItem(keyDb, expectedKey);

    EncryptionKey receivedKey = keyDb.getKey(expectedKey.getKeyId());

    // Manually set creationTime equal since it is automatically set by spanner.
    assertThat(receivedKey)
        .isEqualTo(expectedKey.toBuilder().setCreationTime(receivedKey.getCreationTime()).build());
  }

  @Test
  public void getKey_returnsNotFound() {
    ServiceException exception =
        assertThrows(ServiceException.class, () -> keyDb.getKey("notpresent"));

    assertThat(exception.getErrorCode()).isEqualTo(Code.NOT_FOUND);
  }

  @Test
  public void getActiveKeys_atLimitCount() throws ServiceException {
    putNItemsRandomValues(keyDb, 10);

    awaitAndAssertActiveKeyCount(keyDb, DEFAULT_KEY_ITEM_COUNT);
  }

  @Test
  public void getActiveKeys_whenGreaterThanLimitCount() throws ServiceException {
    putNItemsRandomValues(keyDb, 13);

    awaitAndAssertActiveKeyCount(keyDb, DEFAULT_KEY_ITEM_COUNT);
  }

  @Test
  public void getActiveKeys_whenLessThanLimitCount() throws ServiceException {
    putNItemsRandomValues(keyDb, 3);

    awaitAndAssertActiveKeyCount(keyDb, 3);
  }

  @Test
  public void getActiveKeys_whenEmpty() {
    awaitAndAssertActiveKeyCount(keyDb, 0);
  }

  @Test
  public void getActiveKeys_expiredKeysAreFiltered() throws ServiceException {
    putKeyWithExpiration(keyDb, Instant.now().minusSeconds(2000));

    awaitAndAssertActiveKeyCount(keyDb, 0);
  }

  @Test
  public void getActiveKeys_inactiveKeysAreFiltered() throws ServiceException {
    putKeyWithActivationAndExpirationTimes(
        keyDb, Instant.now().plus(Duration.ofHours(1)), Instant.now().plus(Duration.ofHours(1)));
    awaitAndAssertActiveKeyCount(keyDb, 0);

    putKeyWithActivationAndExpirationTimes(
        keyDb, Instant.now(), Instant.now().plus(Duration.ofHours(1)));
    awaitAndAssertActiveKeyCount(keyDb, 1);
  }

  @Test
  public void getActiveKeys_keysAreSorted() throws ServiceException {
    // Insert items out of expiration time order.
    for (var i : ImmutableList.of(2, 4, 3, 6, 1)) {
      putKeyWithExpiration(keyDb, Instant.now().plus(Duration.ofHours(i)));
    }

    awaitAndAssertActiveKeyCount(keyDb, 5);

    // SpannerKeyDb doesn't use getActiveKeysComparator but other implementations do, ensure the
    // sort order of SpannerKeyDb matches other implementations.
    assertThat(keyDb.getActiveKeys(5)).isInOrder(KeyDbUtil.getActiveKeysComparator());
  }

  @Test
  public void getActiveKeys_doesNotReturnNonAsymmetricKeys() throws Exception {
    // Given
    EncryptionKey asymmetricKey = FakeEncryptionKey.create();
    EncryptionKey asymmetricKey2 = FakeEncryptionKey.create();
    EncryptionKey nonAsymmetricKey1 =
        FakeEncryptionKey.create().toBuilder().clearPublicKey().clearPublicKeyMaterial().build();
    EncryptionKey nonAsymmetricKey2 =
        FakeEncryptionKey.create().toBuilder().clearPublicKey().clearPublicKeyMaterial().build();
    keyDb.createKeys(
        ImmutableList.of(asymmetricKey, asymmetricKey2, nonAsymmetricKey1, nonAsymmetricKey2));

    // When
    ImmutableList<EncryptionKey> keys =
        keyDb.getActiveKeys(KeyDb.DEFAULT_SET_NAME, 100, Instant.now());

    // Then
    ImmutableList<String> ids =
        keys.stream().map(EncryptionKey::getKeyId).collect(toImmutableList());
    assertThat(ids).containsAtLeast(asymmetricKey.getKeyId(), asymmetricKey2.getKeyId());
    assertThat(ids).containsNoneOf(nonAsymmetricKey1.getKeyId(), nonAsymmetricKey2.getKeyId());
  }

  @Test
  public void getAllKeys_throwsServiceError() {
    ServiceException exception = assertThrows(ServiceException.class, () -> keyDb.getAllKeys());

    assertThat(exception.getErrorCode()).isEqualTo(Code.NOT_FOUND);
    assertThat(exception.getErrorReason()).isEqualTo(UNSUPPORTED_OPERATION.name());
  }

  /**
   * Calls keyDb.getActiveKeys() continuously for at most 5 seconds until the assertions succeed and
   * the expectedSize of keys are returned.
   */
  private static void awaitAndAssertActiveKeyCount(SpannerKeyDb keyDb, int expectedSize) {
    // Wait at least 1 second when asserting the expected size is 0 to avoid false positives.
    long minTimeSec = expectedSize == 0 ? 1 : 0;
    await()
        .pollDelay(minTimeSec, TimeUnit.SECONDS)
        .atMost(5, TimeUnit.SECONDS)
        .untilAsserted(
            () -> {
              ImmutableList<EncryptionKey> keys = keyDb.getActiveKeys(DEFAULT_KEY_ITEM_COUNT);

              assertThat(keys).isNotNull();
              assertThat(keys).hasSize(expectedSize);
            });
  }
}
