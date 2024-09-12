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
package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static com.google.scp.shared.util.KeySplitUtil.reconstructXorKeysetHandle;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.HybridDecrypt;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.Mac;
import com.google.inject.Inject;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.testing.FakeKeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Base64;
import java.util.Map.Entry;
import org.junit.Test;

public abstract class CreateSplitKeyTaskBaseTest {

  @Inject protected CreateSplitKeyTaskBase task;
  @Inject protected InMemoryKeyDb keyDb;
  @Inject @KmsKeyAead protected Aead keyEncryptionKeyAead;

  @Test
  public void create_differentTinkTemplates_successfullyReconstructExpectedPrimitives()
      throws Exception {
    // Given
    ImmutableList<Entry<String, Class<?>>> expectedPrimitives =
        new ImmutableMap.Builder<String, Class<?>>()
            .put("DHKEM_X25519_HKDF_SHA256_HKDF_SHA256_CHACHA20_POLY1305_RAW", HybridDecrypt.class)
            .put("HMAC_SHA512_256BITTAG_RAW", Mac.class)
            .build().entrySet().asList();

    // When
    for (Entry<String, Class<?>> primitive : expectedPrimitives) {
      String template = primitive.getKey();
      task.createSplitKey("test-set", template, 1, 1, 1, Instant.now());
    }

    // Then
    ImmutableList<EncryptionKey> keys = task.keyDb.getAllKeys().reverse();
    ImmutableList<byte[]> peerSplits = capturePeerSplits();
    for (int i = 0; i < expectedPrimitives.size(); i++) {
      String template = expectedPrimitives.get(i).getKey();
      Class<?> expectedPrimitive = expectedPrimitives.get(i).getValue();

      byte[] localSplit = keyEncryptionKeyAead.decrypt(
          Base64.getDecoder().decode(keys.get(i).getJsonEncodedKeyset()), new byte[0]);
      byte[] peerSplit = peerSplits.get(i);

      KeysetHandle reconstructed = reconstructXorKeysetHandle(
          ImmutableList.of(
              ByteString.copyFrom(localSplit), ByteString.copyFrom(peerSplit)));

      assertThat(reconstructed.getKeysetInfo().getKeyInfo(0).getTypeUrl())
          .isEqualTo(KeyTemplates.get(template).getTypeUrl());
      assertThat(reconstructed.getPrimitive(expectedPrimitive))
          .isInstanceOf(expectedPrimitive);
    }
  }

  @Test
  public void create_noKeys_createsActiveKeysAndPendingActiveForEach() throws Exception {
    // Given
    assertThat(task.keyDb.getActiveKeys(Integer.MAX_VALUE)).isEmpty();

    // When
    task.create(5, 7, 365);

    // Then
    assertThat(task.keyDb.getActiveKeys(Integer.MAX_VALUE)).hasSize(5);
    assertThat(task.keyDb.getAllKeys()).hasSize(10);
  }

  @Test
  public void create_onlyOneButNotEnough_createsOnlyMissingKey() throws Exception {
    // Given
    task.create(1, 7, 365);
    assertThat(task.keyDb.getActiveKeys(Integer.MAX_VALUE)).hasSize(1);
    assertThat(task.keyDb.getAllKeys()).hasSize(2);

    // When
    task.create(5, 7, 365);

    // Then
    assertThat(task.keyDb.getActiveKeys(Integer.MAX_VALUE)).hasSize(5);
    assertThat(task.keyDb.getAllKeys()).hasSize(10);
  }

  @Test
  public void create_noKeys_createsKeysWithOneDayBuffer() throws Exception {
    // Given
    assertThat(task.keyDb.getActiveKeys(Integer.MAX_VALUE)).isEmpty();

    // When
    task.create(5, 7, 365);

    // Then
    EncryptionKey key = task.keyDb.getActiveKeys(Integer.MAX_VALUE).get(0);
    Instant expiration = Instant.ofEpochMilli(key.getExpirationTime());
    Instant refreshWindowBeforeExpiration =
        expiration.minus(CreateSplitKeyTaskBase.KEY_REFRESH_WINDOW);
    Instant refreshWindowAndSecondBeforeExpiration =
        refreshWindowBeforeExpiration.minus(1, ChronoUnit.SECONDS);

    assertThat(task.keyDb.getActiveKeys(Integer.MAX_VALUE, refreshWindowAndSecondBeforeExpiration))
        .hasSize(5);
    assertThat(task.keyDb.getActiveKeys(Integer.MAX_VALUE, refreshWindowBeforeExpiration))
        .hasSize(10);
    assertThat(task.keyDb.getActiveKeys(Integer.MAX_VALUE, expiration)).hasSize(5);
  }

  @Test
  public void create_differentSetNames_createsDifferentSets() throws Exception {
    // Given
    String setName1 = "set-name-1";
    String setName2 = "set-name-2";

    assertThat(task.keyDb.getActiveKeys(setName1, Integer.MAX_VALUE)).isEmpty();
    assertThat(task.keyDb.getActiveKeys(setName2, Integer.MAX_VALUE)).isEmpty();

    // When
    task.create(setName1, "DHKEM_X25519_HKDF_SHA256_HKDF_SHA256_CHACHA20_POLY1305_RAW", 5, 7, 365);
    task.create(setName2, "HMAC_SHA512_256BITTAG_RAW", 5, 7, 365);

    // Then
    assertThat(
            task.keyDb.getAllKeys().stream()
                .map(EncryptionKey::getSetName)
                .filter(setName1::equals))
        .hasSize(10);
    assertThat(
            task.keyDb.getAllKeys().stream()
                .map(EncryptionKey::getSetName)
                .filter(setName2::equals))
        .hasSize(10);
  }

  protected abstract ImmutableList<byte[]> capturePeerSplits() throws Exception;
}
