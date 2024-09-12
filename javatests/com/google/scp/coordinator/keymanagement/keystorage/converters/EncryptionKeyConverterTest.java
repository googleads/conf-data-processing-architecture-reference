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

package com.google.scp.coordinator.keymanagement.keystorage.converters;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class EncryptionKeyConverterTest {

  @Test
  public void toStorageEncryptionKey_success() {
    String path = "keys/";
    String keyId = "myKey";
    EncryptionKey keyStorageKey =
        EncryptionKey.newBuilder()
            .setName(path + keyId)
            .setSetName("test-set-name")
            .setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY)
            .setPublicKeysetHandle("12345")
            .setPublicKeyMaterial("qwert")
            .setCreationTime(0L)
            .setActivationTime(1L)
            .setExpirationTime(2L)
            .addAllKeyData(ImmutableList.of())
            .build();

    com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey
        result = EncryptionKeyConverter.toStorageEncryptionKey(keyId, keyStorageKey);

    assertThat(path + result.getKeyId()).isEqualTo(keyStorageKey.getName());
    assertThat(result.getSetName()).isEqualTo(keyStorageKey.getSetName());
    assertThat(result.getPublicKey()).isEqualTo(keyStorageKey.getPublicKeysetHandle());
    assertThat(result.getActivationTime()).isEqualTo(keyStorageKey.getActivationTime());
    assertThat(result.getExpirationTime()).isEqualTo(keyStorageKey.getExpirationTime());
  }

  @Test
  public void toStorageEncryptionKey_missingKeyId() {
    String path = "keys/";
    EncryptionKey keyStorageKey =
        EncryptionKey.newBuilder()
            .setName(path)
            .setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY)
            .setPublicKeysetHandle("12345")
            .setPublicKeyMaterial("qwert")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .addAllKeyData(ImmutableList.of())
            .build();

    IllegalArgumentException ex =
        assertThrows(
            IllegalArgumentException.class,
            () -> EncryptionKeyConverter.toStorageEncryptionKey(null, keyStorageKey));

    assertThat(ex.getMessage()).isEqualTo("KeyId cannot be null or empty.");
  }

  @Test
  public void toApiEncryptionKey_noPrivateKeyMaterial() {
    EncryptionKey encryptionKey =
        EncryptionKeyConverter.toApiEncryptionKey(FakeEncryptionKey.create());
    // {@link FakeEncryptionKey} should always give a non-empty list of {@code keyData} so that this
    // test is meaningful.  If this assertion fails, it means we should fix {@code
    // FakeEncryptionKey.create} to provide more representative test data.
    assertThat(encryptionKey.getKeyDataList()).isNotEmpty();
    boolean privateKeyMaterialEmpty =
        encryptionKey.getKeyDataList().stream()
            .allMatch(keyData -> keyData.getKeyMaterial().isEmpty());
    assertThat(privateKeyMaterialEmpty).isTrue();
  }

  @Test
  public void toApiEncryptionKey_correctName() {
    String keyId = "abc";
    String path = "encryptionKeys/abc";
    EncryptionKey encryptionKey =
        EncryptionKeyConverter.toApiEncryptionKey(
            FakeEncryptionKey.create().toBuilder().setKeyId(keyId).build());
    assertThat(encryptionKey.getName()).isEqualTo(path);
  }

  @Test
  public void toApiEncryptionKey_roundTrip() {
    var original = FakeEncryptionKey.create().toBuilder().setJsonEncodedKeyset("").build();
    var converted =
        EncryptionKeyConverter.toStorageEncryptionKey(
            original.getKeyId(), EncryptionKeyConverter.toApiEncryptionKey(original));
    // The top-level URI doesn't exist on the API model, so ignore it.
    var originalNoUri = original.toBuilder().clearKeyEncryptionKeyUri().clearSetName().build();
    assertThat(converted).isEqualTo(originalNoUri);
  }

  @Test
  public void toApiEncryptionKey_badKeyType() {
    var encryptionKey =
        FakeEncryptionKey.create().toBuilder().setKeyType("HYPER_PARTY_KEYSPLIT").build();

    IllegalArgumentException ex =
        assertThrows(
            IllegalArgumentException.class,
            () -> EncryptionKeyConverter.toApiEncryptionKey(encryptionKey));

    assertThat(ex.getMessage()).isEqualTo("Unrecognized KeyType: HYPER_PARTY_KEYSPLIT");
  }

  @Test
  public void toApiEncryptionKey_unspecifiedKeyType() {
    var storageKey = FakeEncryptionKey.create().toBuilder().setKeyType("").build();

    EncryptionKey encryptionKey = EncryptionKeyConverter.toApiEncryptionKey(storageKey);

    assertThat(encryptionKey.getEncryptionKeyType())
        .isEqualTo(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY);
  }
}
