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

package com.google.scp.coordinator.keymanagement.shared.util;

import static com.google.common.collect.ImmutableMap.toImmutableMap;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.common.base.Joiner;
import com.google.common.collect.ImmutableMap;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.crypto.tink.signature.EcdsaSignKeyManager;
import com.google.crypto.tink.signature.SignatureConfig;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.time.Instant;
import java.util.Base64;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.stream.Stream;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class KeySplitDataUtilTest {

  @Before
  public void setUp() throws GeneralSecurityException {
    SignatureConfig.register();
  }

  /** Tests format of added keysplit data */
  @Test
  public void addKeySplitData_correctKeySplitData() throws GeneralSecurityException {
    EncryptionKey baseKey = FakeEncryptionKey.create();
    int numKeySplitData = baseKey.getKeySplitDataList().size();
    KeysetHandle keysetHandle = KeysetHandle.generateNew(EcdsaSignKeyManager.ecdsaP256Template());
    PublicKeySign publicKeySign = keysetHandle.getPrimitive(PublicKeySign.class);
    PublicKeyVerify publicKeyVerify =
        keysetHandle.getPublicKeysetHandle().getPrimitive(PublicKeyVerify.class);

    EncryptionKey finalKey =
        KeySplitDataUtil.addKeySplitData(baseKey, "blah", Optional.of(publicKeySign));
    // assumes that the new keysplitdata is added at the end of the list
    List<KeySplitData> fromOriginalKeySplitData =
        finalKey.getKeySplitDataList().subList(0, numKeySplitData);
    KeySplitData newKeySplitData = finalKey.getKeySplitData(numKeySplitData);

    assertThat(fromOriginalKeySplitData).isEqualTo(baseKey.getKeySplitDataList());
    assertThat(newKeySplitData.getKeySplitKeyEncryptionKeyUri()).isEqualTo("blah");

    // ${key_id}|${iso 8601 creation_time}|${public_key_material}
    Instant creationTime = Instant.ofEpochMilli(finalKey.getCreationTime());
    byte[] expectedMessage =
        Joiner.on("|")
            .join(finalKey.getKeyId(), creationTime.toString(), finalKey.getPublicKeyMaterial())
            .getBytes(StandardCharsets.UTF_8);
    // should throw if key fails to verify
    publicKeyVerify.verify(
        Base64.getDecoder().decode(newKeySplitData.getPublicKeySignature()), expectedMessage);
  }

  /** Tests format of added keysplit data if there is no signature */
  @Test
  public void addKeySplitData_correctNoSignature() throws GeneralSecurityException {
    EncryptionKey baseKey = FakeEncryptionKey.create().toBuilder().clearKeySplitData().build();
    int numKeySplitData = baseKey.getKeySplitDataList().size();

    EncryptionKey finalKey = KeySplitDataUtil.addKeySplitData(baseKey, "blah", Optional.empty());
    KeySplitData newKeySplitData = finalKey.getKeySplitData(0);

    assertThat(newKeySplitData.getKeySplitKeyEncryptionKeyUri()).isEqualTo("blah");
    assertThat(newKeySplitData.getPublicKeySignature()).isEmpty();
  }

  /** Tests that the signatures added can be verified */
  @Test
  public void addKeySplitData_roundtrip() throws GeneralSecurityException {
    EncryptionKey baseKey = FakeEncryptionKey.create();
    ImmutableMap<String, KeysetHandle> keysetHandles =
        Stream.generate(KeySplitDataUtilTest::unsafeCreateKeysetHandle)
            .limit(10)
            .collect(
                toImmutableMap(
                    keysetHandle -> UUID.randomUUID().toString(), keysetHandle -> keysetHandle));
    ImmutableMap<String, PublicKeyVerify> publicKeyVerifiers =
        keysetHandles.entrySet().stream()
            .collect(
                toImmutableMap(
                    Map.Entry::<String, KeysetHandle>getKey,
                    entry -> getPublicKeyVerify(entry.getValue())));

    EncryptionKey finalKey = baseKey;
    for (Map.Entry<String, KeysetHandle> keysetHandle : keysetHandles.entrySet()) {
      PublicKeySign publicKeySign = keysetHandle.getValue().getPrimitive(PublicKeySign.class);
      finalKey =
          KeySplitDataUtil.addKeySplitData(
              finalKey, keysetHandle.getKey(), Optional.of(publicKeySign));
    }

    // should throw if keys fail to verify
    KeySplitDataUtil.verifyEncryptionKeySignatures(finalKey, publicKeyVerifiers);
  }

  /** Should throw if a signature is invalid */
  @Test
  public void verifyEncryptionKeySignatures_throwIfInvalid() throws GeneralSecurityException {
    EncryptionKey baseKey = FakeEncryptionKey.create();
    KeysetHandle keysetHandle = KeysetHandle.generateNew(EcdsaSignKeyManager.ecdsaP256Template());
    PublicKeyVerify publicKeyVerify =
        keysetHandle.getPublicKeysetHandle().getPrimitive(PublicKeyVerify.class);

    EncryptionKey finalKey = KeySplitDataUtil.addKeySplitData(baseKey, "blah", Optional.empty());

    GeneralSecurityException ex =
        assertThrows(
            GeneralSecurityException.class,
            () ->
                KeySplitDataUtil.verifyEncryptionKeySignatures(
                    finalKey, ImmutableMap.of("blah", publicKeyVerify)));
    assertThat(ex).hasMessageThat().contains("blah");
    assertThat(ex).hasMessageThat().contains("failed to verify");
  }

  /** Should throw if a signature is not found */
  @Test
  public void verifyEncryptionKeySignatures_throwIfNotFound() throws GeneralSecurityException {
    EncryptionKey baseKey = FakeEncryptionKey.create();
    KeysetHandle keysetHandle = KeysetHandle.generateNew(EcdsaSignKeyManager.ecdsaP256Template());
    PublicKeyVerify publicKeyVerify =
        keysetHandle.getPublicKeysetHandle().getPrimitive(PublicKeyVerify.class);

    GeneralSecurityException ex =
        assertThrows(
            GeneralSecurityException.class,
            () ->
                KeySplitDataUtil.verifyEncryptionKeySignatures(
                    baseKey, ImmutableMap.of("blah", publicKeyVerify)));
    assertThat(ex).hasMessageThat().contains("blah");
    assertThat(ex).hasMessageThat().contains("not found");
  }

  /** Needed for testing with Stream.generate() */
  private static KeysetHandle unsafeCreateKeysetHandle() {
    try {
      return KeysetHandle.generateNew(EcdsaSignKeyManager.ecdsaP256Template());
    } catch (GeneralSecurityException e) {
      throw new RuntimeException(e);
    }
  }

  /** Needed for testing with collect() */
  private static PublicKeyVerify getPublicKeyVerify(KeysetHandle keysetHandle) {
    try {
      return keysetHandle.getPublicKeysetHandle().getPrimitive(PublicKeyVerify.class);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException(e);
    }
  }
}
