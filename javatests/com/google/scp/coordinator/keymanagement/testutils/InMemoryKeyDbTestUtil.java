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
import static java.time.Instant.now;
import static java.time.temporal.ChronoUnit.DAYS;
import static java.util.UUID.randomUUID;

import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetEncryptedPrivateKeyResponseProto.GetEncryptedPrivateKeyResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyStatusProto.EncryptionKeyStatus;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.util.KeyParams;
import com.google.scp.shared.util.PublicKeyConversionUtil;
import java.security.GeneralSecurityException;
import java.util.stream.IntStream;

public final class InMemoryKeyDbTestUtil {

  public static final Long SECONDS_PER_DAY = 86400L;
  public static final Long CACHE_CONTROL_MAX = 604800L;

  static {
    try {
      HybridConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Failed to register hybrid config for Tink.");
    }
  }

  private InMemoryKeyDbTestUtil() {}

  /** Adds keyCount-number of keys to InMemoryDB */
  public static void addRandomKeysToKeyDb(int keyCount, InMemoryKeyDb keyDb) {
    IntStream.range(0, keyCount).forEach(unused -> createRandomKey(keyDb));
  }

  /**
   * Puts one Key item with random values to InMemoryKeyDb instance. Returns randomly-generated
   * EncryptionKey
   */
  public static EncryptionKey createRandomKey(InMemoryKeyDb keyDb) {
    String publicKey;
    String publicKeyMaterial;
    try {
      KeysetHandle key = KeysetHandle.generateNew(KeyParams.getDefaultKeyTemplate());
      publicKey = toJsonCleartext(key.getPublicKeysetHandle());
      publicKeyMaterial = PublicKeyConversionUtil.getPublicKey(key.getPublicKeysetHandle());
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
    return createKey(
        keyDb,
        /* keyId= */ randomUUID().toString(),
        /* publicKey= */ publicKey,
        /* publicKeyMaterial= */ publicKeyMaterial,
        /* privateKey= */ randomUUID().toString(),
        /* keyEncryptionKeyUri= */ randomUUID().toString(),
        /* creationTime= */ now().toEpochMilli(),
        /* expirationTime= */ now().plus(7, DAYS).toEpochMilli());
  }

  /**
   * Puts one Key item with random values to InMemoryKeyDb instance. Returns randomly-generated
   * EncryptionKey
   */
  public static EncryptionKey createKey(
      InMemoryKeyDb keyDb,
      String keyId,
      String publicKey,
      String publicKeyMaterial,
      String privateKey,
      String keyEncryptionKeyUri,
      Long creationTime,
      Long expirationTime) {
    return createKey(
        keyDb,
        keyId,
        publicKey,
        publicKeyMaterial,
        privateKey,
        /* status= */ EncryptionKeyStatus.ACTIVE,
        keyEncryptionKeyUri,
        creationTime,
        expirationTime);
  }

  /**
   * Puts one Key item with parameter values to InMemoryKeyDb instance. Returns EncryptionKey if
   * successful
   */
  public static EncryptionKey createKey(
      InMemoryKeyDb keyDb,
      String keyId,
      String publicKey,
      String publicKeyMaterial,
      String privateKey,
      EncryptionKeyStatus status,
      String keyEncryptionKeyUri,
      Long creationTime,
      Long expirationTime) {

    try {
      EncryptionKey.Builder encryptionKey =
          EncryptionKey.newBuilder().setKeyId(keyId).setStatus(status);

      if (publicKey != null) {
        encryptionKey.setPublicKey(publicKey);
      }
      if (publicKeyMaterial != null) {
        encryptionKey.setPublicKeyMaterial(publicKeyMaterial);
      }
      if (privateKey != null) {
        encryptionKey.setJsonEncodedKeyset(privateKey);
      }
      if (keyEncryptionKeyUri != null) {
        encryptionKey.setKeyEncryptionKeyUri(keyEncryptionKeyUri);
      }
      if (creationTime != null) {
        encryptionKey.setCreationTime(creationTime);
      }
      if (expirationTime != null) {
        encryptionKey.setExpirationTime(expirationTime);
      }

      keyDb.createKey(encryptionKey.build());
      return keyDb.getKey(keyId);
    } catch (ServiceException e) {
      throw new IllegalArgumentException("Could not create key", e);
    }
  }

  /** Builds {@link EncodedPublicKey} given its properties */
  public static EncodedPublicKey expectedEncodedPublicKey(String keyId, String publicKey) {
    return EncodedPublicKey.newBuilder().setId(keyId).setKey(publicKey).build();
  }

  /** Builds {@link EncodedPublicKey} representation of {@link EncryptionKey} */
  public static EncodedPublicKey expectedEncodedPublicKey(EncryptionKey encryptionKey) {
    return EncodedPublicKey.newBuilder()
        .setId(encryptionKey.getKeyId())
        .setKey(encryptionKey.getPublicKeyMaterial())
        .build();
  }

  /** Builds string representation of {@link GetEncryptedPrivateKeyResponse} */
  public static String expectedPrivateKeyResponseBody(String privateKeyId, String privateKey) {
    return "{\n  \"name\": \"privateKeys/"
        + privateKeyId
        + "\",\n  \"jsonEncodedKeyset\": \""
        + privateKey
        + "\"\n}";
  }

  public static String expectedErrorResponseBody(int code, String message, String reason) {
    return String.format(
        "{\n"
            + "  \"code\": %d,\n"
            + "  \"message\": \"%s\",\n"
            + "  \"details\": [{\n"
            + "    \"reason\": \"%s\",\n"
            + "    \"domain\": \"\",\n"
            + "    \"metadata\": {\n"
            + "    }\n"
            + "  }]\n"
            + "}",
        code, message, reason);
  }
}
