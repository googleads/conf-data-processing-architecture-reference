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

/** Handles requests to PublicKeyService and returns HTTP Response. */
public final class PublicKeyServiceHttpFunction extends PublicKeyServiceHttpFunctionBase {

  /**
   * Creates a new instance of the {@code PublicKeyServiceHttpFunction} class with the given {@link
   * GetPublicKeysRequestHandler}.
   */
  public PublicKeyServiceHttpFunction(GetPublicKeysRequestHandler getPublicKeysRequestHandler) {
    super(getPublicKeysRequestHandler);
  }

  /** Creates a new instance of the {@code PublicKeyServiceHttpFunction} class. */
  public PublicKeyServiceHttpFunction() {
    this(
        Guice.createInjector(new GcpKeyServiceModule())
            .getInstance(GetPublicKeysRequestHandler.class));
  }
}
