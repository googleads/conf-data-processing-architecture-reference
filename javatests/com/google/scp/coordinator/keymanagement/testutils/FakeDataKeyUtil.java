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

import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.DataKeyEncryptionUtil.decryptWithDataKey;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.DataKeyEncryptionUtil.encryptWithDataKey;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toBinaryCiphertext;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AeadConfig;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.util.Base64Util;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.HashMap;
import java.util.UUID;

/**
 * Test util class for creating valid {@link DataKey} objects and that can be decrypted using {@link
 * #getAeadSelector()}
 */
public final class FakeDataKeyUtil {

  private static Aead defaultKeyEncryptionKey;
  // Contains the AEAD for each data key created with using this class in order to allow the data
  // key to be conveniently encrypted or decrypted.
  private static final HashMap<String, Aead> aeadByUri = new HashMap<String, Aead>();

  static {
    try {
      AeadConfig.register();
      defaultKeyEncryptionKey =
          KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM")).getPrimitive(Aead.class);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error initializing tink.");
    }
  }

  private FakeDataKeyUtil() {}

  /**
   * Returns a valid Data Key that can be encrypted with using {@link #encryptString} or decrypted
   * with using {@link #getAeadSelector}
   */
  public static DataKey createDataKey() {
    return createDataKey(defaultKeyEncryptionKey, generateAeadUri());
  }

  /**
   * Creates a data key encrypted with a custom AEAD -- registers the AEAD so that getAeadSelector
   * will return the same AEAD when passed the DataKeyKekUri. This allows the returned DataKey to be
   * used with {@link #encryptString(DataKey,String)}
   */
  public static DataKey createDataKey(Aead kekAead, String kekUri) {
    try {
      var keysetHandle = KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM"));
      var keyBinaryText = toBinaryCiphertext(keysetHandle, kekAead);
      registerDataKey(kekAead, kekUri);

      return DataKey.newBuilder()
          .setEncryptedDataKeyKekUri(kekUri)
          .setEncryptedDataKey(Base64Util.toBase64String(keyBinaryText))
          .build();
    } catch (GeneralSecurityException | IOException e) {
      throw new IllegalStateException("Failed to create fake Data key", e);
    }
  }

  /**
   * Registers {@code kekUri} to be used with {@code encryptString} and {@code decryptString}.
   * Automatically performed for keys created with {@link #createDataKey()}.
   */
  public static void registerDataKey(Aead kekAead, String kekUri) {
    aeadByUri.put(kekUri, kekAead);
  }

  /**
   * Helper method for encrypting an unencoded string using a DataKey and returning the
   * base64-encoded result.
   */
  public static String encryptString(DataKey dataKey, String cleartextMessage, String publicKey)
      throws Exception {
    var cleartextBytes = ByteString.copyFrom(cleartextMessage.getBytes());
    var encryptedBytes = encryptWithDataKey(getAeadSelector(), dataKey, cleartextBytes, publicKey);

    return Base64Util.toBase64String(encryptedBytes);
  }

  public static byte[] decryptString(DataKey dataKey, String ciphertextMessage, String publicKey)
      throws Exception {
    var ciphertextBytes = Base64Util.fromBase64String(ciphertextMessage);
    var decryptedBytes = decryptWithDataKey(getAeadSelector(), dataKey, ciphertextBytes, publicKey);

    return decryptedBytes.toByteArray();
  }

  /** Returns an AEAD selector that can decrypt DataKeys created with {@link #createDataKey}. */
  public static CloudAeadSelector getAeadSelector() {
    return (String uri) -> aeadByUri.get(uri);
  }

  private static String generateAeadUri() {
    return String.format("psuedo-kms://%s", UUID.randomUUID());
  }
}
