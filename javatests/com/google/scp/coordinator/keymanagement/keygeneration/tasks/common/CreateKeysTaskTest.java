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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.Range;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeyTemplate.OutputPrefixType;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.security.GeneralSecurityException;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class CreateKeysTaskTest {
  @Rule public final Acai acai = new Acai(KeyGenerationTestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Inject private InMemoryKeyDb keyDb;
  @Inject private CreateKeysTask task;
  @Inject @KeyEncryptionKeyUri private String keyEncryptionKeyUri;
  @Mock Aead mockAead;

  @Test
  public void createKeys_success() throws ServiceException {
    int keysToCreate = 5;

    task.createKeys(/* cnt= */ keysToCreate, /* validityInDays= */ 7, /* ttlInDays= */ 365);

    ImmutableList<EncryptionKey> keys = keyDb.getAllKeys();
    assertThat(keys.size()).isEqualTo(keysToCreate);
    assertThat(keys.get(0).getKeyEncryptionKeyUri()).isEqualTo(keyEncryptionKeyUri);
  }

  @Test
  public void createKeys_keyGenerationError() throws ServiceException, GeneralSecurityException {
    int keysToCreate = 5;
    when(mockAead.encrypt(any(), any())).thenThrow(new GeneralSecurityException());

    CreateKeysTask taskWithMock = new CreateKeysTask(keyDb, mockAead, keyEncryptionKeyUri);
    ServiceException ex =
        assertThrows(
            ServiceException.class,
            () ->
                taskWithMock.createKeys(
                    /* cnt= */ keysToCreate, /* validityInDays= */ 7, /* ttlInDays= */ 365));

    assertThat(ex).hasCauseThat().isInstanceOf(GeneralSecurityException.class);
    assertThat(ex.getErrorCode()).isEqualTo(Code.INTERNAL);
    ImmutableList<EncryptionKey> keys = keyDb.getAllKeys();
    assertThat(keys.size()).isEqualTo(0);
    verify(mockAead).encrypt(any(), any());
  }

  @Test
  public void createKeys_properties() throws Exception {
    task.createKeys(/* cnt= */ 1, /* validityInDays=*/ 1, /* ttlInDays= */ 30);

    var keys = keyDb.getAllKeys();

    assertThat(keys.size()).isEqualTo(1);
    EncryptionKey key = keys.get(0);
    // Must be an HPKE key
    assertThat(key.getJsonEncodedKeyset())
        .contains("type.googleapis.com/google.crypto.tink.HpkePrivateKey");
    // Must be a raw key type
    assertThat(key.getJsonEncodedKeyset()).contains(OutputPrefixType.RAW.toString());

    // Must have a creationTime of now
    var now = Instant.now().toEpochMilli();
    assertThat(key.getCreationTime()).isIn(Range.closed(now - 1000, now));

    // Must have a validity of 1 day
    var oneDayMillis = Instant.now().plus(1, ChronoUnit.DAYS).toEpochMilli();
    assertThat(key.getExpirationTime()).isIn(Range.closed(oneDayMillis - 1000, oneDayMillis));

    // Must have a ttl of 30 days
    var thirtyDaysSec = Instant.now().plus(30, ChronoUnit.DAYS).getEpochSecond();
    assertThat(key.getTtlTime()).isIn(Range.closed(thirtyDaysSec - 2, thirtyDaysSec));
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

  private void insertKeyWithExpiration(Instant expirationTime) throws ServiceException {
    keyDb.createKey(FakeEncryptionKey.withExpirationTime(expirationTime));
  }
}
