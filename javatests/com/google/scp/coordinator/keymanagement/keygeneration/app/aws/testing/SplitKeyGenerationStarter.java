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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing;

import com.google.common.util.concurrent.ServiceManager;
import com.google.inject.Guice;
import com.google.inject.Inject;
import com.google.inject.Injector;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.AwsSplitKeyGenerationArgs;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.AwsSplitKeyGenerationModule;
import java.util.Optional;

/**
 * Wrapper for running an {@link AwsSplitKeyGenerationApplication} in tests.
 *
 * @see SplitKeyGenerationArgsLocalStackProvider for making it easy to inject this in LocalStack
 *     tests.
 * @see SplitKeyQueueTestHelper for making it easy to interact with the started server.
 */
public final class SplitKeyGenerationStarter {

  /** Present if the service manager is started and healthy, otherwise empty. */
  private Optional<ServiceManager> currentServiceManager = Optional.empty();

  private final AwsSplitKeyGenerationArgs args;

  @Inject
  public SplitKeyGenerationStarter(AwsSplitKeyGenerationArgs args) {
    this.args = args;
  }

  /**
   * Starts the SplitKeyGenerationApplication, throwing if it's already started.
   *
   * <p>Each subsequent invocation starts a new instance with a new injector tree.
   *
   * @throws IllegalStateException If started while an existing service manager is running.
   */
  public void start() {
    if (currentServiceManager.isPresent()) {
      throw new IllegalStateException(
          "Attempted to start a SplitKeyGenerationStart while another was already running.");
    }

    var serviceManager = createServiceManager(args);
    serviceManager.startAsync().awaitHealthy();

    currentServiceManager = Optional.of(serviceManager);
  }

  /**
   * Stops the SplitKeyGenerationApplication, does not throw if the application is already stopped
   * to allow this to safely be used in an @After block.
   */
  public void stop() {
    if (currentServiceManager.isEmpty()) {
      return;
    }
    var serviceManager = currentServiceManager.get();
    serviceManager.stopAsync().awaitStopped();

    currentServiceManager = Optional.empty();
  }

  private static ServiceManager createServiceManager(AwsSplitKeyGenerationArgs args) {
    Injector injector = Guice.createInjector(new AwsSplitKeyGenerationModule(args));
    return injector.getInstance(ServiceManager.class);
  }
}
