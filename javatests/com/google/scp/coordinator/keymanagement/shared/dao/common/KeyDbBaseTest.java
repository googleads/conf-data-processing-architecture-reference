/*
 * Copyright 2023 Google LLC
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
package com.google.scp.coordinator.keymanagement.shared.dao.common;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.ALREADY_EXISTS;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.time.Duration;
import java.time.Instant;
import javax.inject.Inject;
import org.junit.Test;

public abstract class KeyDbBaseTest {

  @Inject protected KeyDb db;

  @Test
  public void getActiveKeys_specificMoments_returnsExpected() throws Exception {
    // Given
    String setName = "test-set-name";
    Instant t0 = Instant.now();
    Instant t1 = t0.plusSeconds(1);
    Instant t2 = t0.plusSeconds(2);
    Instant t3 = t0.plusSeconds(3);
    Instant t4 = t0.plusSeconds(4);

    db.createKey(fakeEncryptionKey(setName, t1, t3));
    db.createKey(fakeEncryptionKey(setName, t1, t3));

    db.createKey(fakeEncryptionKey(setName, t2, t3));
    db.createKey(fakeEncryptionKey(setName, t2, t3));

    db.createKey(fakeEncryptionKey(setName, t2, t4));
    db.createKey(fakeEncryptionKey(setName, t2, t4));

    db.createKey(fakeEncryptionKey(setName, t3, t4));
    db.createKey(fakeEncryptionKey(setName, t3, t4));

    // When/Then
    assertThat(db.getActiveKeys(setName, Integer.MAX_VALUE, t0)).hasSize(0);
    assertThat(db.getActiveKeys(setName, Integer.MAX_VALUE, t1)).hasSize(2);
    assertThat(db.getActiveKeys(setName, Integer.MAX_VALUE, t2)).hasSize(6);
    assertThat(db.getActiveKeys(setName, Integer.MAX_VALUE, t3)).hasSize(4);
    assertThat(db.getActiveKeys(setName, Integer.MAX_VALUE, t4)).hasSize(0);
  }

  @Test
  public void getActiveKeys_specificSetName_returnsExpected() throws Exception {
    // Given
    String setName1 = "test-set-name-1";
    String setName2 = "test-set-name-2";

    db.createKey(fakeEncryptionKey());
    db.createKey(fakeEncryptionKey());
    db.createKey(fakeEncryptionKey(setName1));
    db.createKey(fakeEncryptionKey(setName1));
    db.createKey(fakeEncryptionKey(setName2));
    db.createKey(fakeEncryptionKey(setName1));
    db.createKey(fakeEncryptionKey(setName2));

    // When/Then
    assertThat(db.getActiveKeys(setName1, Integer.MAX_VALUE)).hasSize(3);
    assertThat(db.getActiveKeys(setName2, Integer.MAX_VALUE)).hasSize(2);
  }

  @Test
  public void getActiveKeys_defaultSetName_equivalentToUnsetSetName() throws Exception {
    // Given
    String nonDefaultName = "non-default";

    db.createKey(fakeEncryptionKey());
    db.createKey(fakeEncryptionKey(nonDefaultName));
    db.createKey(fakeEncryptionKey(KeyDb.DEFAULT_SET_NAME));
    db.createKey(fakeEncryptionKey());
    db.createKey(fakeEncryptionKey(nonDefaultName));
    db.createKey(fakeEncryptionKey(KeyDb.DEFAULT_SET_NAME));
    db.createKey(fakeEncryptionKey(KeyDb.DEFAULT_SET_NAME));

    // When/Then
    assertThat(db.getActiveKeys(Integer.MAX_VALUE)).hasSize(5);
    assertThat(db.getActiveKeys(KeyDb.DEFAULT_SET_NAME, Integer.MAX_VALUE)).hasSize(5);
    assertThat(db.getActiveKeys(nonDefaultName, Integer.MAX_VALUE)).hasSize(2);
  }

  @Test
  public void createKey_overwriteSuccess() throws Exception {
    String keyId = "asdf";
    EncryptionKey key1 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setJsonEncodedKeyset("12345")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();
    EncryptionKey key2 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setJsonEncodedKeyset("67890")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    db.createKey(key1);
    db.createKey(key2);
    EncryptionKey result = db.getKey(keyId);

    assertThat(clearCreationTime(result)).isEqualTo(key2);
  }

  @Test
  public void listRecentKeys_specificMaxAge_returnsExpectedKeys() throws Exception {
    // Given
    long creationDelayMs = 2500;
    long latencyToleranceMs = creationDelayMs / 5;

    // Using two different methods to influence creation_time's written to the database because
    // different database implementations have different creation_time semantics when writing to
    // persisting keys. e.g. Dynamo uses the creation_time values set in the arguments while
    // Spanner uses insertion times set by the database engine.
    EncryptionKey k0 =
        FakeEncryptionKey.withCreationTime(Instant.now().minusMillis(2 * creationDelayMs));
    EncryptionKey k1 =
        FakeEncryptionKey.withCreationTime(Instant.now().minusMillis(creationDelayMs));
    EncryptionKey k2 = FakeEncryptionKey.withCreationTime(Instant.now());

    db.createKey(k0);
    Thread.sleep(creationDelayMs);
    db.createKey(k1);
    Thread.sleep(creationDelayMs);
    db.createKey(k2);

    k0 = db.getKey(k0.getKeyId());
    k1 = db.getKey(k1.getKeyId());
    k2 = db.getKey(k2.getKeyId());

    // When
    ImmutableList<EncryptionKey> keys0 =
        db.listRecentKeys(
                Duration.between(Instant.ofEpochMilli(k0.getCreationTime()), Instant.now())
                    .plusMillis(latencyToleranceMs))
            .collect(toImmutableList());
    ImmutableList<EncryptionKey> keys1 =
        db.listRecentKeys(
                Duration.between(Instant.ofEpochMilli(k1.getCreationTime()), Instant.now())
                    .plusMillis(latencyToleranceMs))
            .collect(toImmutableList());
    ImmutableList<EncryptionKey> keys2 =
        db.listRecentKeys(
                Duration.between(Instant.ofEpochMilli(k2.getCreationTime()), Instant.now())
                    .plusMillis(latencyToleranceMs))
            .collect(toImmutableList());

    // Then
    assertThat(db.listRecentKeys(Duration.ZERO).collect(toImmutableList())).isEmpty();
    assertThat(keys0).hasSize(3);
    assertThat(keys1).hasSize(2);
    assertThat(keys2).hasSize(1);

    assertThat(keys0).containsExactly(k0, k1, k2);
    assertThat(keys1).containsExactly(k1, k2);
    assertThat(keys2).containsExactly(k2);
  }

  @Test
  public void listRecentKeys_defaultSetName_equivalentToUnsetSetName() throws Exception {
    // Given
    String nonDefaultName = "non-default";

    db.createKey(fakeEncryptionKey());
    db.createKey(fakeEncryptionKey(nonDefaultName));
    db.createKey(fakeEncryptionKey(KeyDb.DEFAULT_SET_NAME));
    db.createKey(fakeEncryptionKey());
    db.createKey(fakeEncryptionKey(nonDefaultName));
    db.createKey(fakeEncryptionKey(KeyDb.DEFAULT_SET_NAME));
    db.createKey(fakeEncryptionKey(KeyDb.DEFAULT_SET_NAME));

    // When/Then
    assertThat(db.listRecentKeys(Duration.ofSeconds(999)).toArray()).hasLength(5);
    assertThat(db.listRecentKeys(KeyDb.DEFAULT_SET_NAME, Duration.ofSeconds(999)).toArray())
        .hasLength(5);
    assertThat(db.listRecentKeys(nonDefaultName, Duration.ofSeconds(999)).toArray()).hasLength(2);
  }

  @Test
  public void createKey_overwriteFailWithException() throws ServiceException {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();
    String keyId = "asdf";
    EncryptionKey key1 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setJsonEncodedKeyset("12345")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();
    EncryptionKey key2 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setJsonEncodedKeyset("67890")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    keyDb.createKey(key1);
    ServiceException e = assertThrows(ServiceException.class, () -> keyDb.createKey(key2, false));
    assertThat(e.getErrorCode()).isEqualTo(ALREADY_EXISTS);
  }

  protected EncryptionKey clearCreationTime(EncryptionKey key) {
    return key.toBuilder().setCreationTime(0L).build();
  }

  private static EncryptionKey fakeEncryptionKey() {
    return FakeEncryptionKey.createBuilderWithDefaults().build();
  }

  private static EncryptionKey fakeEncryptionKey(String setName) {
    return FakeEncryptionKey.createBuilderWithDefaults().setSetName(setName).build();
  }

  private static EncryptionKey fakeEncryptionKey(
      String setName, Instant activationTime, Instant expirationTime) {
    return FakeEncryptionKey.withActivationAndExpirationTimes(activationTime, expirationTime)
        .toBuilder()
        .setSetName(setName)
        .build();
  }
}
