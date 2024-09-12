/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp.testing;

import com.google.crypto.tink.aead.AeadConfig;
import com.google.inject.Guice;
import com.google.scp.coordinator.keymanagement.keystorage.service.gcp.CreateKeyRequestHandler;
import com.google.scp.coordinator.keymanagement.keystorage.service.gcp.KeyStorageServiceHttpFunctionBase;
import java.security.GeneralSecurityException;

/**
 * KeyStorageServiceHttpFunction with custom environment for local invocation using functions
 * framework.
 */
public class LocalKeyStorageServiceHttpFunction extends KeyStorageServiceHttpFunctionBase {

  public static final String KEYSET_HANDLE_ENCODE_STRING_ENV_NAME = "KEYSET_HANDLE_ENCODE";

  static {
    try {
      AeadConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException(e);
    }
  }

  public LocalKeyStorageServiceHttpFunction() {
    super(
        Guice.createInjector(new KeyStorageHttpFunctionModule(KEYSET_HANDLE_ENCODE_STRING_ENV_NAME))
            .getInstance(CreateKeyRequestHandler.class));
  }
}
