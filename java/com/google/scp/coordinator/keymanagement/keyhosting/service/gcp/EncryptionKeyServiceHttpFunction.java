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

package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import com.google.inject.Guice;
import com.google.inject.Injector;

/** Handles requests to EncryptionKeyService and returns HTTP Response. */
public final class EncryptionKeyServiceHttpFunction extends EncryptionKeyServiceHttpFunctionBase {

  /**
   * Creates a new instance of the {@code EncryptionKeyServiceHttpFunction} class with the given
   * {@link GetEncryptionKeyRequestHandler} and {@link ListRecentEncryptionKeysRequestHandler}.
   */
  public EncryptionKeyServiceHttpFunction(
      GetEncryptionKeyRequestHandler getEncryptionKeyRequestHandler,
      ListRecentEncryptionKeysRequestHandler ListRecentEncryptionKeysRequestHandler) {
    super(getEncryptionKeyRequestHandler, ListRecentEncryptionKeysRequestHandler);
  }

  /** Creates a new instance of the {@code PrivateKeyServiceHttpFunction} class. */
  public EncryptionKeyServiceHttpFunction() {
    this(Guice.createInjector(new GcpKeyServiceModule()));
  }

  /**
   * Creates a new instance of the {@code PrivateKeyServiceHttpFunction} class with the given {@link
   * Injector}.
   */
  private EncryptionKeyServiceHttpFunction(Injector injector) {
    this(
        injector.getInstance(GetEncryptionKeyRequestHandler.class),
        injector.getInstance(ListRecentEncryptionKeysRequestHandler.class));
  }
}
