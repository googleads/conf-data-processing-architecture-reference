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

package com.google.scp.operator.cpio.cryptoclient.local;

import com.google.crypto.tink.BinaryKeysetReader;
import com.google.crypto.tink.CleartextKeysetHandle;
import com.google.crypto.tink.HybridDecrypt;
import com.google.crypto.tink.HybridEncrypt;
import com.google.crypto.tink.KeysetHandle;
import com.google.inject.Inject;
import com.google.scp.operator.cpio.cryptoclient.HybridEncryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.local.LocalFileHybridEncryptionKeyServiceModule.DecryptionKeyFilePath;
import com.google.scp.operator.cpio.cryptoclient.model.ErrorReason;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.GeneralSecurityException;

/**
 * Reads {@link KeysetHandle} from {@link com.google.crypto.tink.proto.Keyset} proto in binary wire
 * format.
 */
public final class LocalFileHybridEncryptionKeyService implements HybridEncryptionKeyService {

  private final Path decryptionKeyFilePath;

  /** Creates a new instance of the {@code LocalFileHybridEncryptionKeyService} class. */
  @Inject
  public LocalFileHybridEncryptionKeyService(@DecryptionKeyFilePath Path decryptionKeyPath) {
    this.decryptionKeyFilePath = decryptionKeyPath;
  }

  @Override
  public HybridDecrypt getDecrypter(String unusedId) throws KeyFetchException {
    try {
      var keysetHandle =
          CleartextKeysetHandle.read(
              BinaryKeysetReader.withInputStream(Files.newInputStream(decryptionKeyFilePath)));
      return keysetHandle.getPrimitive(HybridDecrypt.class);
    } catch (IOException | GeneralSecurityException e) {
      throw new KeyFetchException(e, ErrorReason.UNKNOWN_ERROR);
    }
  }

  @Override
  public HybridEncrypt getEncrypter(String unusedId) throws KeyFetchException {
    try {
      var keysetHandle =
          CleartextKeysetHandle.read(
              BinaryKeysetReader.withInputStream(Files.newInputStream(decryptionKeyFilePath)));
      return keysetHandle.getPublicKeysetHandle().getPrimitive(HybridEncrypt.class);
    } catch (IOException | GeneralSecurityException e) {
      throw new KeyFetchException(e, ErrorReason.UNKNOWN_ERROR);
    }
  }
}
