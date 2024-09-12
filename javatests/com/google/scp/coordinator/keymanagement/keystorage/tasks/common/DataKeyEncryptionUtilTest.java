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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.common;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.DataKeyEncryptionUtil.decryptWithDataKey;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.DataKeyEncryptionUtil.encryptWithDataKey;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toBinaryCiphertext;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AeadConfig;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.util.Base64Util;
import java.security.GeneralSecurityException;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class DataKeyEncryptionUtilTest {
  private static final String kekUri = "aws-kms://arn:aws:example";
  private static final String associatedData = "associated_data";

  static {
    try {
      AeadConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error initializing tink.");
    }
  }

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private CloudAeadSelector aeadSelector;

  // Per-test data key for encryption/decryption.
  private DataKey dataKey;
  // Unencrypted Aead key that's stored encrypted within {@code dataKey}.
  private Aead originalDataKeyAead;

  @Before
  public void setup() throws Exception {
    var keyEncryptionKey = generateKeyEncryptionKey();
    var keysetHandle = KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM"));

    dataKey = generateDataKey(keysetHandle, keyEncryptionKey);
    originalDataKeyAead = keysetHandle.getPrimitive(Aead.class);

    // Default to returning the keyEncryptionKey used to encrypt the encryptedDataKey.
    when(aeadSelector.getAead(kekUri)).thenReturn(keyEncryptionKey);
  }

  @Test
  public void encryptWithDataKey_success() throws Exception {
    var payload = ByteString.copyFrom("foo".getBytes());

    var encryptedResult = encryptWithDataKey(aeadSelector, dataKey, payload, associatedData);

    assertThat(encryptedResult).isNotEmpty();
    assertThat(encryptedResult).isNotEqualTo(payload);
    verify(aeadSelector, times(1)).getAead(kekUri);

    var decryptedPayload =
        originalDataKeyAead.decrypt(encryptedResult.toByteArray(), associatedData.getBytes(UTF_8));
    assertThat(ByteString.copyFrom(decryptedPayload)).isEqualTo(payload);
  }

  @Test
  public void encryptWithDataKey_decryptionFailure() throws Exception {
    // Return a different encryption key than the keyEncryptionKey.
    when(aeadSelector.getAead(kekUri)).thenReturn(generateKeyEncryptionKey());
    var payload = ByteString.copyFrom("foo".getBytes());

    var e =
        assertThrows(
            GeneralSecurityException.class,
            () -> encryptWithDataKey(aeadSelector, dataKey, payload, associatedData));

    assertThat(e).hasMessageThat().isEqualTo("Failed to decrypt data key");
    assertThat(e).hasCauseThat().isInstanceOf(GeneralSecurityException.class);
    assertThat(e).hasCauseThat().hasMessageThat().contains("decryption failed");
  }

  @Test
  public void decryptWithDataKey_success() throws Exception {
    var payload = ByteString.copyFrom("baz".getBytes());
    var encryptedPayload =
        ByteString.copyFrom(
            originalDataKeyAead.encrypt(payload.toByteArray(), associatedData.getBytes(UTF_8)));

    var decryptedPayload =
        decryptWithDataKey(aeadSelector, dataKey, encryptedPayload, associatedData);

    assertThat(decryptedPayload).isEqualTo(payload);
    verify(aeadSelector, times(1)).getAead(kekUri);
  }

  @Test
  public void decryptWithDataKey_decryptionFailure() throws Exception {
    // Return a different encryption key than the keyEncryptionKey.
    when(aeadSelector.getAead(kekUri)).thenReturn(generateKeyEncryptionKey());
    var payload = ByteString.copyFrom("foo".getBytes());
    var encryptedPayload =
        ByteString.copyFrom(
            originalDataKeyAead.encrypt(payload.toByteArray(), associatedData.getBytes(UTF_8)));

    var e =
        assertThrows(
            GeneralSecurityException.class,
            () -> decryptWithDataKey(aeadSelector, dataKey, encryptedPayload, associatedData));

    assertThat(e).hasMessageThat().isEqualTo("Failed to decrypt data key");
    assertThat(e).hasCauseThat().isInstanceOf(GeneralSecurityException.class);
    assertThat(e).hasCauseThat().hasMessageThat().contains("decryption failed");
  }

  @Test
  public void decryptWithDataKey_roundTripSuccess() throws Exception {
    var payload = ByteString.copyFrom("foo".getBytes());

    var secretMessage = encryptWithDataKey(aeadSelector, dataKey, payload, associatedData);
    var decryptedPayload = decryptWithDataKey(aeadSelector, dataKey, secretMessage, associatedData);

    assertThat(decryptedPayload).isEqualTo(payload);
  }

  /**
   * Returns a data key to wrap the provided KeysetHandle. Encrypts the key using {@code
   * keyEncryptionKey}.
   */
  private static DataKey generateDataKey(KeysetHandle keysetHandle, Aead keyEncryptionKey)
      throws Exception {
    var encryptedDataKey =
        Base64Util.toBase64String(toBinaryCiphertext(keysetHandle, keyEncryptionKey));

    return DataKey.newBuilder()
        .setEncryptedDataKey(encryptedDataKey)
        .setEncryptedDataKeyKekUri(kekUri)
        .build();
  }

  private static Aead generateKeyEncryptionKey() throws Exception {
    // Use a different key type than the data key to prevent mixing them up.
    return KeysetHandle.generateNew(KeyTemplates.get("CHACHA20_POLY1305")).getPrimitive(Aead.class);
  }
}
