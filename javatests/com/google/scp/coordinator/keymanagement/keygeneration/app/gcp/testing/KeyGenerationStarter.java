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

package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.testing;

import com.google.inject.Guice;
import com.google.inject.Inject;
import com.google.inject.Injector;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.KeyGenerationArgs;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.KeyGenerationModule;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.PubSubListener;

/** Starts a KeyGeneration pubsub listener. */
public final class KeyGenerationStarter {

  private final KeyGenerationArgs args;

  @Inject
  public KeyGenerationStarter(KeyGenerationArgs args) {
    this.args = args;
  }

  public void start() {
    PubSubListener pubSubListner = createPubSubListener(args);
    new Thread(() -> pubSubListner.start()).start();
  }

  private static PubSubListener createPubSubListener(KeyGenerationArgs args) {
    Injector injector = Guice.createInjector(new KeyGenerationModule(args));
    return injector.getInstance(PubSubListener.class);
  }
}