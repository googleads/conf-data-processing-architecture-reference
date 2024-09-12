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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.collect.ImmutableMap.toImmutableMap;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.Range;
import com.google.common.collect.Streams;
import com.google.crypto.tink.Aead;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient.KeyStorageServiceException;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.testing.FakeKeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTaskBaseTest;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.PeerCoordinatorKmsKeyAead;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyStatusProto.EncryptionKeyStatus;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.security.GeneralSecurityException;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Base64;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

public abstract class GcpCreateSplitKeyTaskTestBase extends CreateSplitKeyTaskBaseTest {
  @Inject protected InMemoryKeyDb keyDb;
  @Inject @KmsKeyAead protected Aead keyEncryptionKeyAead;
  @Inject @KeyEncryptionKeyUri protected String keyEncryptionKeyUri;
  @Inject @PeerCoordinatorKmsKeyAead protected Aead peerCoordinatorKeyEncryptionKeyAead;
  @Inject protected FakeKeyStorageClient keyStorageClient;

  // TODO: Refactor common test code to shared class
  @Test
  public void createSplitKey_success() throws Exception {
    int keysToCreate = 1;
    int expectedExpiryInDays = 10;
    int expectedTtlInDays = 20;
    var encryptionKeyCaptor = ArgumentCaptor.forClass(EncryptionKey.class);
    var encryptedKeySplitCaptor = ArgumentCaptor.forClass(String.class);

    task.createSplitKey(keysToCreate, expectedExpiryInDays, expectedTtlInDays, Instant.now());

    ImmutableList<EncryptionKey> keys = keyDb.getAllKeys();
    assertThat(keys).hasSize(keysToCreate);

    EncryptionKey key = keys.get(0);

    // Validate that the key split decrypts (currently no associated data)
    keyEncryptionKeyAead.decrypt(
        Base64.getDecoder().decode(key.getJsonEncodedKeyset()), new byte[0]);

    // Misc metadata
    assertThat(key.getStatus()).isEqualTo(EncryptionKeyStatus.ACTIVE);
    assertThat(key.getKeyEncryptionKeyUri()).isEqualTo(keyEncryptionKeyUri);
    assertThat(key.getKeyType()).isEqualTo("MULTI_PARTY_HYBRID_EVEN_KEYSPLIT");

    // Properties about Key Split Data
    assertThat(key.getKeySplitDataList()).hasSize(2);
    ImmutableMap<String, KeySplitData> keySplitDataMap =
        key.getKeySplitDataList().stream()
            .collect(
                toImmutableMap(
                    KeySplitData::getKeySplitKeyEncryptionKeyUri,
                    keySplitDataItem -> keySplitDataItem));
    KeySplitData keySplitDataA = keySplitDataMap.get(keyEncryptionKeyUri);
    KeySplitData keySplitDataB = keySplitDataMap.get(FakeKeyStorageClient.KEK_URI);

    // Coordinator A's KeySplitData
    assertThat(keySplitDataA).isNotNull();
    assertThat(keySplitDataA.getKeySplitKeyEncryptionKeyUri()).isEqualTo(keyEncryptionKeyUri);
    // TODO: Verify signature
    // Coordinator B's KeySplitData
    assertThat(keySplitDataB).isNotNull();
    assertThat(keySplitDataB.getKeySplitKeyEncryptionKeyUri())
        .isEqualTo(FakeKeyStorageClient.KEK_URI);
    // TODO: Verify signature

    // Must have a creationTime of now
    var now = Instant.now().toEpochMilli();
    assertThat(key.getCreationTime()).isIn(Range.closed(now - 1000, now));

    // Must have expected expiration time
    var dayInMilli = Instant.now().plus(expectedExpiryInDays, ChronoUnit.DAYS).toEpochMilli();
    assertThat(key.getExpirationTime()).isIn(Range.closed(dayInMilli - 1000, dayInMilli));

    // Must match expected ttl
    var ttlInSec = Instant.now().plus(expectedTtlInDays, ChronoUnit.DAYS).getEpochSecond();
    assertThat(key.getTtlTime()).isIn(Range.closed(ttlInSec - 2, ttlInSec));

    verify(keyStorageClient, times(keysToCreate))
        .createKey(encryptionKeyCaptor.capture(), encryptedKeySplitCaptor.capture());

    assertThat(encryptionKeyCaptor.getValue().getKeyId()).isEqualTo(key.getKeyId());

    // Validate that the key split decrypts with peer coordinator KEK
    peerCoordinatorKeyEncryptionKeyAead.decrypt(
        Base64.getDecoder().decode(encryptedKeySplitCaptor.getValue()),
        Base64.getDecoder().decode(key.getPublicKeyMaterial()));
  }

  @Test
  public void createSplitKey_keyGenerationError()
      throws ServiceException, GeneralSecurityException {
    int keysToCreate = 5;

    Mockito.doThrow(new GeneralSecurityException())
        .when(keyEncryptionKeyAead)
        .encrypt(any(), any());

    ServiceException ex =
        assertThrows(
            ServiceException.class, () -> task.createSplitKey(keysToCreate, 10, 20, Instant.now()));

    assertThat(ex).hasCauseThat().isInstanceOf(GeneralSecurityException.class);
    assertThat(ex.getErrorCode()).isEqualTo(Code.INTERNAL);
    ImmutableList<EncryptionKey> keys = keyDb.getAllKeys();
    assertThat(keys).isEmpty();
    verify(keyEncryptionKeyAead, times(1)).encrypt(any(), any());
  }

  /** Ensure that even if we fail after two attempted generations, we store two keys */
  @Test
  public void createSplitKey_keyGenerationInterrupted()
      throws ServiceException, KeyStorageServiceException {
    int keysToCreate = 5;

    when(keyStorageClient.createKey(any(), any()))
        .thenCallRealMethod()
        .thenCallRealMethod()
        .thenThrow(new KeyStorageServiceException("Failure", new GeneralSecurityException()));

    ServiceException ex =
        assertThrows(
            ServiceException.class, () -> task.createSplitKey(keysToCreate, 10, 20, Instant.now()));

    assertThat(ex).hasCauseThat().isInstanceOf(KeyStorageServiceException.class);
    ImmutableList<EncryptionKey> keys = keyDb.getAllKeys();
    assertThat(keys).hasSize(3);
  }

  /** Tests that invoking replaceExpiringKeys multiple times doesn't create multiple keys. */
  @Test
  public void replaceExpiringKeys_duplicate() throws Exception {
    task.replaceExpiringKeys(
        /* numDesiredKeys= */ 3, /* validityInDays= */ 2, /* ttlInDays= */ 365);

    assertThat(keyDb.getAllKeys().size()).isEqualTo(3);

    task.replaceExpiringKeys(
        /* numDesiredKeys= */ 3, /* validityInDays= */ 2, /* ttlInDays= */ 365);

    assertThat(keyDb.getAllKeys().size()).isEqualTo(3);
  }

  @Test
  public void replaceExpiringKeys_refreshesAllKeys() throws Exception {
    // Create 5 keys that will expire soon
    for (var i = 0; i < 5; i++) {
      insertKeyWithExpiration(Instant.now().plus(Duration.ofHours(20)));
    }

    assertThat(keyDb.getAllKeys().size()).isEqualTo(5);

    // All 5 keys should be refreshed.
    task.replaceExpiringKeys(
        /* numDesiredKeys= */ 5, /* validityInDays= */ 7, /* ttlInDays= */ 365);

    assertThat(keyDb.getAllKeys().size()).isEqualTo(10);

    // There are 5 fresh keys so this should be a no-op.
    task.replaceExpiringKeys(
        /* numDesiredKeys= */ 5, /* validityInDays= */ 7, /* ttlInDays= */ 365);

    assertThat(keyDb.getAllKeys().size()).isEqualTo(10);
  }

  @Test
  public void replaceExpiringKeys_refreshesSomeKeys() throws Exception {
    // Create 1 key inside the refresh window and one key outside the refreshWindow
    insertKeyWithExpiration(Instant.now().plus(Duration.ofHours(1))); // inside
    insertKeyWithExpiration(Instant.now().plus(Duration.ofDays(5))); // outside

    assertThat(keyDb.getAllKeys().size()).isEqualTo(2);

    task.replaceExpiringKeys(
        /* numDesiredKeys= */ 2, /* validityInDays= */ 5, /* ttlInDays= */ 365);

    // Only one key is inside the refresh window so only one key should be created.
    assertThat(keyDb.getAllKeys().size()).isEqualTo(3);
  }

  protected void insertKeyWithExpiration(Instant expirationTime) throws ServiceException {
    keyDb.createKey(FakeEncryptionKey.withExpirationTime(expirationTime));
  }

  @Override
  protected ImmutableList<byte[]> capturePeerSplits() throws Exception {
    var encryptionKeyCaptor = ArgumentCaptor.forClass(EncryptionKey.class);
    var encryptedKeySplitCaptor = ArgumentCaptor.forClass(String.class);

    verify(keyStorageClient, atLeastOnce())
        .createKey(encryptionKeyCaptor.capture(), encryptedKeySplitCaptor.capture());

    return Streams.zip(
            encryptionKeyCaptor.getAllValues().stream(),
            encryptedKeySplitCaptor.getAllValues().stream(),
            (encryptionKey, encryptedKeySplit) -> {
              try {
                return peerCoordinatorKeyEncryptionKeyAead.decrypt(
                    Base64.getDecoder().decode(encryptedKeySplit),
                    Base64.getDecoder().decode(encryptionKey.getPublicKeyMaterial()));
              } catch (GeneralSecurityException e) {
                throw new RuntimeException(e);
              }
            })
        .collect(toImmutableList());
  }
}
