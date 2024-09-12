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

import com.google.common.base.Converter;
import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.BinaryKeysetWriter;
import com.google.crypto.tink.CleartextKeysetHandle;
import com.google.crypto.tink.JsonKeysetReader;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.KeysetWriter;
import com.google.crypto.tink.proto.HpkePublicKey;
import com.google.crypto.tink.subtle.Base64;
import com.google.inject.Inject;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.security.GeneralSecurityException;

/** Converts between ImmutableList of {@link EncryptionKey} and {@link EncodedPublicKey} */
public final class EncodedPublicKeyListConverter
    extends Converter<ImmutableList<EncryptionKey>, ImmutableList<EncodedPublicKey>> {

  public final Mode mode;

  /** The public key output mode. */
  public enum Mode {
    // Outputs the raw key material only.
    LEGACY,
    // Outputs the serialized binary Tink keyset.
    TINK,
    // Outputs the key as an explicit object.
    RAW
  }

  @Inject
  @Deprecated
  public EncodedPublicKeyListConverter() {
    this.mode = Mode.LEGACY;
  }

  /**
   * @param mode the public key encoding mode to use.
   */
  public EncodedPublicKeyListConverter(Mode mode) {
    this.mode = mode;
  }

  @Override
  protected ImmutableList<EncodedPublicKey> doForward(ImmutableList<EncryptionKey> encryptionKeys) {
    return encryptionKeys.stream()
        .map(
            publicKey -> {
              EncodedPublicKey.Builder key =
                  EncodedPublicKey.newBuilder().setId(publicKey.getKeyId());
              switch (mode) {
                case LEGACY:
                  key.setKey(publicKey.getPublicKeyMaterial());
                  break;
                case RAW:
                  key.setHpkePublicKey(toHpkePublicKey(publicKey));
                  break;
                case TINK:
                default:
                  key.setTinkBinary(toTinkBinary(publicKey));
              }
              return key.build();
            })
        .collect(ImmutableList.toImmutableList());
  }

  private static HpkePublicKey toHpkePublicKey(EncryptionKey key) {
    try {
      return HpkePublicKey.parseFrom(
          JsonKeysetReader.withString(key.getPublicKey())
              .read()
              .getKey(0)
              .getKeyData()
              .getValue()
              .toByteArray());
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  private static String toTinkBinary(EncryptionKey key) {
    try {
      ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
      KeysetHandle keysetHandle =
          CleartextKeysetHandle.read(JsonKeysetReader.withString(key.getPublicKey()));
      KeysetWriter writer = BinaryKeysetWriter.withOutputStream(outputStream);
      CleartextKeysetHandle.write(keysetHandle, writer);
      return Base64.encode(outputStream.toByteArray());
    } catch (GeneralSecurityException | IOException e) {
      throw new RuntimeException(e);
    }
  }

  @Override
  protected ImmutableList<EncryptionKey> doBackward(
      ImmutableList<EncodedPublicKey> encodedPublicKeys) {
    throw new UnsupportedOperationException();
  }
}
