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

package com.google.scp.coordinator.keymanagement.testutils;

import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toJsonCleartext;

import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.crypto.tink.signature.EcdsaSignKeyManager;
import com.google.crypto.tink.signature.SignatureConfig;
import com.google.scp.coordinator.keymanagement.shared.util.KeySplitDataUtil;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.proto.ProtoUtil;
import com.google.scp.shared.util.KeyParams;
import com.google.scp.shared.util.PublicKeyConversionUtil;
import java.security.GeneralSecurityException;
import java.time.Duration;
import java.time.Instant;
import java.util.Optional;
import java.util.Random;
import java.util.UUID;

/** Util methods for creating {@link EncryptionKey}s with fake data. */
public final class FakeEncryptionKey {
  private FakeEncryptionKey() {}

  private static final PublicKeySign PUBLIC_KEY_SIGN;
  public static final PublicKeyVerify PUBLIC_KEY_VERIFY;

  static {
    try {
      SignatureConfig.register();
      KeysetHandle signatureKey = KeysetHandle.generateNew(EcdsaSignKeyManager.ecdsaP256Template());
      PUBLIC_KEY_SIGN = signatureKey.getPrimitive(PublicKeySign.class);
      PUBLIC_KEY_VERIFY = signatureKey.getPublicKeysetHandle().getPrimitive(PublicKeyVerify.class);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error initializing Fake Key Storage Client signature keys.");
    }
    try {
      HybridConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Failed to register hybrid config for Tink.");
    }
  }

  public static EncryptionKey.Builder createBuilderWithDefaults() {
    Random random = new Random();
    Instant now = Instant.now();
    EncryptionKeyType keyType =
        EncryptionKeyType.values()[
            random.nextInt(ProtoUtil.getValidEnumValues(EncryptionKeyType.class).size())];
    // This is the URI for the owner of this EncryptionKey.
    // It must match to one of the URIs in the key split data.
    String encryptionKeyUri = UUID.randomUUID().toString();
    EncryptionKey baseKey =
        setPublicKeys(EncryptionKey.newBuilder())
            .setKeyId(UUID.randomUUID().toString())
            .setJsonEncodedKeyset(UUID.randomUUID().toString())
            .setKeyEncryptionKeyUri(encryptionKeyUri)
            .setCreationTime(now.toEpochMilli())
            .setActivationTime(now.toEpochMilli())
            .setExpirationTime(now.plus(Duration.ofDays(7)).toEpochMilli())
            .setTtlTime(now.plus(Duration.ofDays(30)).getEpochSecond())
            .setKeyType(keyType.name())
            .build();
    try {
      EncryptionKey keyWithSignatures =
          KeySplitDataUtil.addKeySplitData(
              KeySplitDataUtil.addKeySplitData(
                  baseKey, encryptionKeyUri, Optional.of(PUBLIC_KEY_SIGN)),
              UUID.randomUUID().toString(),
              Optional.empty());
      return keyWithSignatures.toBuilder();
    } catch (GeneralSecurityException e) {
      throw new IllegalStateException("Signature failure");
    }
  }

  /**
   * Creates a key with realistic timestamps but fake key materials. The generated key will be
   * active and expire in the future.
   */
  public static EncryptionKey create() {
    return createBuilderWithDefaults().build();
  }

  /** Creates a key which actives and expires at the specified time. */
  public static EncryptionKey withActivationAndExpirationTimes(
      Instant activationTime, Instant expirationTime) {
    return create().toBuilder()
        .setActivationTime(activationTime.toEpochMilli())
        .setExpirationTime(expirationTime.toEpochMilli())
        .build();
  }

  /** Creates a key which expires at the specified time. */
  public static EncryptionKey withExpirationTime(Instant expirationTime) {
    return create().toBuilder().setExpirationTime(expirationTime.toEpochMilli()).build();
  }

  /** Creates a key with a specified creationTime. */
  public static EncryptionKey withCreationTime(Instant creationTime) {
    return create().toBuilder().setCreationTime(creationTime.toEpochMilli()).build();
  }

  public static KeySplitData createKeySplitData(
      EncryptionKeyType keyType, String encryptionKeyUri) {
    return KeySplitData.newBuilder()
        .setKeySplitKeyEncryptionKeyUri(encryptionKeyUri)
        .setPublicKeySignature(UUID.randomUUID().toString())
        .build();
  }

  public static EncryptionKey withKeyId(String keyId) {
    return createBuilderWithDefaults().setKeyId(keyId).build();
  }

  private static EncryptionKey.Builder setPublicKeys(EncryptionKey.Builder encryptionKey) {
    try {
      KeysetHandle key = KeysetHandle.generateNew(KeyParams.getDefaultKeyTemplate());
      return encryptionKey
          .setPublicKey(toJsonCleartext(key.getPublicKeysetHandle()))
          .setPublicKeyMaterial(PublicKeyConversionUtil.getPublicKey(key.getPublicKeysetHandle()));
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }
}
