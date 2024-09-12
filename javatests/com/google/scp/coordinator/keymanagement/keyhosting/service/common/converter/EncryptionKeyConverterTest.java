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

package com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import java.util.UUID;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class EncryptionKeyConverterTest {

  @Test
  public void toApiEncryptionKey_hasPrivateKeyMaterial_singleKey() {
    String encryptionKeyUri = UUID.randomUUID().toString();
    ImmutableList<KeySplitData> keySplitData =
        ImmutableList.of(
            FakeEncryptionKey.createKeySplitData(
                EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY, encryptionKeyUri));
    var storageKey =
        FakeEncryptionKey.create().toBuilder()
            .setKeyEncryptionKeyUri(encryptionKeyUri)
            // Need to clear before adding, otherwise there will be duplicate KeySplitData elements.
            .clearKeySplitData()
            .addAllKeySplitData(keySplitData)
            .build();

    EncryptionKey encryptionKey = EncryptionKeyConverter.toApiEncryptionKey(storageKey);

    assertThat(encryptionKey.getKeyDataList().size()).isEqualTo(1);
    // Only the key data with the URI matching the key owner should have its key material populated.
    assertThat(encryptionKey.getKeyDataList().get(0).getKeyMaterial())
        .isEqualTo(storageKey.getJsonEncodedKeyset());
  }

  @Test
  public void toApiEncryptionKey_hasPrivateKeyMaterial_legacyKey() {
    String encryptionKeyUri = UUID.randomUUID().toString();
    var storageKey =
        FakeEncryptionKey.create().toBuilder()
            .setKeyEncryptionKeyUri(encryptionKeyUri)
            // Need to clear before adding, otherwise there will be duplicate KeySplitData elements.
            .clearKeySplitData()
            .addAllKeySplitData(ImmutableList.of())
            .build();

    EncryptionKey encryptionKey = EncryptionKeyConverter.toApiEncryptionKey(storageKey);

    assertThat(encryptionKey.getKeyDataList().size()).isEqualTo(1);
    // Only the key data with the URI matching the key owner should have its key material populated.
    assertThat(encryptionKey.getKeyDataList().get(0).getKeyMaterial())
        .isEqualTo(storageKey.getJsonEncodedKeyset());
  }

  @Test
  public void toApiEncryptionKey_hasPrivateKeyMaterial_multiKey() {
    String encryptionKeyUri = UUID.randomUUID().toString();
    ImmutableList<KeySplitData> keySplitData =
        ImmutableList.of(
            FakeEncryptionKey.createKeySplitData(
                EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT, encryptionKeyUri),
            FakeEncryptionKey.createKeySplitData(
                EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT, UUID.randomUUID().toString()));
    var storageKey =
        FakeEncryptionKey.create().toBuilder()
            .setKeyEncryptionKeyUri(encryptionKeyUri)
            // Need to clear before adding, otherwise there will be duplicate KeySplitData elements.
            .clearKeySplitData()
            .addAllKeySplitData(keySplitData)
            .build();

    EncryptionKey encryptionKey = EncryptionKeyConverter.toApiEncryptionKey(storageKey);

    assertThat(encryptionKey.getKeyDataList().size()).isEqualTo(2);
    // Only the key data with the URI matching the key owner should have its key material populated.
    var keyDatasWithMatchingUri =
        encryptionKey.getKeyDataList().stream()
            .filter(
                keyData ->
                    keyData.getKeyEncryptionKeyUri().equals(storageKey.getKeyEncryptionKeyUri()))
            .collect(ImmutableList.toImmutableList());
    assertThat(keyDatasWithMatchingUri.size()).isEqualTo(1);
    assertThat(keyDatasWithMatchingUri.get(0).getKeyMaterial())
        .isEqualTo(storageKey.getJsonEncodedKeyset());
  }

  @Test
  public void toApiEncryptionKey_noMatchingEncryptionUri() {
    String encryptionKeyUri = UUID.randomUUID().toString();
    ImmutableList<KeySplitData> keySplitData =
        ImmutableList.of(
            FakeEncryptionKey.createKeySplitData(
                EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT, UUID.randomUUID().toString()),
            FakeEncryptionKey.createKeySplitData(
                EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT, UUID.randomUUID().toString()));
    var storageKey =
        FakeEncryptionKey.create().toBuilder()
            .setKeyEncryptionKeyUri("123")
            // Need to clear before adding, otherwise there will be duplicate KeySplitData elements.
            .clearKeySplitData()
            .addAllKeySplitData(keySplitData)
            .build();

    EncryptionKey encryptionKey = EncryptionKeyConverter.toApiEncryptionKey(storageKey);

    assertThat(encryptionKey.getKeyDataList().size()).isEqualTo(2);
    // None of the key datas should have a key material.
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

  @Test
  public void toApiEncryptionKey_allSetFields() {
    String keyId = "abc";
    String path = "encryptionKeys/abc";
    String publicKey = "public-key";
    String publicKeyMaterial = "public key material";
    String encryptionKeyUri = UUID.randomUUID().toString();
    long activationTime = 12345;
    long creationTime = 54321;
    long expirationTime = 67890;
    long ttlTime = 98760;

    ImmutableList<KeySplitData> keySplitData =
        ImmutableList.of(
            FakeEncryptionKey.createKeySplitData(
                EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT, encryptionKeyUri),
            FakeEncryptionKey.createKeySplitData(
                EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT, encryptionKeyUri));

    var storageKey =
        FakeEncryptionKey.create().toBuilder()
            .setKeyId(keyId)
            .setPublicKey(publicKey)
            .setPublicKeyMaterial(publicKeyMaterial)
            .setKeyEncryptionKeyUri(encryptionKeyUri)
            // Need to clear before adding, otherwise there will be duplicate KeySplitData elements.
            .clearKeySplitData()
            .addAllKeySplitData(keySplitData)
            .setKeyType("MULTI_PARTY_HYBRID_EVEN_KEYSPLIT")
            .setActivationTime(activationTime)
            .setCreationTime(creationTime)
            .setExpirationTime(expirationTime)
            .setTtlTime(ttlTime)
            .build();

    EncryptionKey encryptionKey = EncryptionKeyConverter.toApiEncryptionKey(storageKey);

    assertThat(encryptionKey.getName()).isEqualTo(path);
    assertThat(encryptionKey.getPublicKeysetHandle()).isEqualTo(publicKey);
    assertThat(encryptionKey.getPublicKeyMaterial()).isEqualTo(publicKeyMaterial);
    assertThat(encryptionKey.getActivationTime()).isEqualTo(activationTime);
    assertThat(encryptionKey.getCreationTime()).isEqualTo(creationTime);
    assertThat(encryptionKey.getExpirationTime()).isEqualTo(expirationTime);
    assertThat(encryptionKey.getTtlTime()).isEqualTo(ttlTime);
    assertThat(encryptionKey.getEncryptionKeyType())
        .isEqualTo(EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT);
    assertThat(encryptionKey.getKeyDataList()).hasSize(2);

    boolean privateKeyMaterialMatching =
        encryptionKey.getKeyDataList().stream()
            .allMatch(
                keyData -> keyData.getKeyMaterial().equals(storageKey.getJsonEncodedKeyset()));
    assertThat(privateKeyMaterialMatching).isTrue();
  }
}
