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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing.SplitKeyGenerationArgsLocalStackProvider.KEY_COUNT;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing.SplitKeyGenerationStarter;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing.SplitKeyQueueTestHelper;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.SequenceKeyIdFactory;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAKeyDbTableName;
import com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorBKeyDbTableName;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.stream.IntStream;
import javax.inject.Inject;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public abstract class SplitKeyGenerationIntegrationTestBase {
  @Inject @CoordinatorAKeyDbTableName private DynamoKeyDb keyDbA;
  @Inject @CoordinatorBKeyDbTableName private DynamoKeyDb keyDbB;
  @Inject SplitKeyGenerationStarter splitKeyGenerationStarter;
  @Inject SplitKeyQueueTestHelper splitKeyQueueHelper;

  @Before
  public void setUp() {
    splitKeyGenerationStarter.start();
  }

  @After
  public void tearDown() {
    splitKeyGenerationStarter.stop();
  }

  @Test
  public void splitKeyGeneration_emptyKeyDb_createsExpectedKeys() throws Exception {
    // Given
    assertThat(keyDbA.getAllKeys()).isEmpty();
    assertThat(keyDbB.getAllKeys()).isEmpty();

    // When
    splitKeyQueueHelper.triggerKeyGeneration();
    splitKeyQueueHelper.waitForEmptyQueue();

    // Then
    assertThat(keyDbA.getActiveKeys(Integer.MAX_VALUE)).hasSize(KEY_COUNT);
    assertThat(keyDbB.getActiveKeys(Integer.MAX_VALUE)).hasSize(KEY_COUNT);

    assertThat(getPendingActiveKeys(keyDbA)).hasSize(KEY_COUNT);
    assertThat(getPendingActiveKeys(keyDbB)).hasSize(KEY_COUNT);

    for (EncryptionKey key : keyDbA.getAllKeys()) {
      assertExpectedKeySplits(key, keyDbB.getKey(key.getKeyId()));
    }
  }

  @Test
  public void splitKeyGeneration_notEnoughActiveKeysAndNoPendingActiveKeys_createsOnlyMissingKeys()
      throws Exception {
    SequenceKeyIdFactory sequenceKeyIdFactory = new SequenceKeyIdFactory();
    // Given
    var numInitialKeys = 1;
    var expirationTime = Instant.now().plus(7, ChronoUnit.DAYS);
    var existingExpiringKeys =
        IntStream.range(0, numInitialKeys)
            .mapToObj(
                unused -> {
                  try {
                    return FakeEncryptionKey.createBuilderWithDefaults()
                        .setKeyId(sequenceKeyIdFactory.getNextKeyId(keyDbA))
                        .setExpirationTime(expirationTime.toEpochMilli())
                        .build();
                  } catch (ServiceException e) {
                    throw new RuntimeException(e);
                  }
                })
            .collect(toImmutableList());
    keyDbA.createKeys(existingExpiringKeys);
    keyDbB.createKeys(existingExpiringKeys);
    assertThat(numInitialKeys).isLessThan(KEY_COUNT);
    assertThat(keyDbA.getAllKeys()).hasSize(numInitialKeys);
    assertThat(keyDbB.getAllKeys()).hasSize(numInitialKeys);

    // When
    splitKeyQueueHelper.triggerKeyGeneration();
    splitKeyQueueHelper.waitForEmptyQueue();

    // Then
    assertThat(keyDbA.getActiveKeys(Integer.MAX_VALUE)).hasSize(KEY_COUNT);
    assertThat(keyDbB.getActiveKeys(Integer.MAX_VALUE)).hasSize(KEY_COUNT);

    // Check when active keys expire, there are enough replacement keys.
    for (var key : keyDbA.getActiveKeys(Integer.MAX_VALUE)) {
      assertThat(keyDbA.getActiveKeys(KEY_COUNT, Instant.ofEpochMilli(key.getExpirationTime())))
          .hasSize(KEY_COUNT);
      assertThat(keyDbB.getActiveKeys(KEY_COUNT, Instant.ofEpochMilli(key.getExpirationTime())))
          .hasSize(KEY_COUNT);
    }
  }

  private ImmutableList<EncryptionKey> getPendingActiveKeys(DynamoKeyDb db) throws Exception {
    long now = Instant.now().toEpochMilli();
    return db.getAllKeys().stream()
        .filter(key -> key.getActivationTime() > now)
        .collect(toImmutableList());
  }

  private static void assertExpectedKeySplits(
      EncryptionKey coordinatorAKey, EncryptionKey coordinatorBKey) {
    assertThat(coordinatorAKey.getKeyId()).isEqualTo(coordinatorBKey.getKeyId());
    assertThat(coordinatorAKey.getExpirationTime()).isEqualTo(coordinatorBKey.getExpirationTime());
    assertThat(coordinatorAKey.getKeyEncryptionKeyUri())
        .isNotEqualTo(coordinatorBKey.getKeyEncryptionKeyUri());
    // Each key should have 2 splits.
    assertThat(coordinatorAKey.getKeySplitDataList()).hasSize(2);
    assertThat(coordinatorBKey.getKeySplitDataList()).hasSize(2);
    // Each split should have one entry matching its coordinator's KEK.
    assertThat(
            coordinatorAKey.getKeySplitDataList().stream()
                .filter(
                    keySplit ->
                        keySplit
                            .getKeySplitKeyEncryptionKeyUri()
                            .equals(coordinatorAKey.getKeyEncryptionKeyUri()))
                .count())
        .isEqualTo(1);
    assertThat(
            coordinatorBKey.getKeySplitDataList().stream()
                .filter(
                    keySplit ->
                        keySplit
                            .getKeySplitKeyEncryptionKeyUri()
                            .equals(coordinatorBKey.getKeyEncryptionKeyUri()))
                .count())
        .isEqualTo(1);
  }

  // TODO(b/239100631): Add test for failure to reach key storage service.
}
