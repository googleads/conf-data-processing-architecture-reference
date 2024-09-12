/*
 * Copyright 2024 Google LLC
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

package com.google.scp.shared.gcp.util;

import com.google.cloud.functions.CloudEventsFunction;
import com.google.protobuf.Message;
import io.cloudevents.CloudEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Base class to implement a cloud function handler which receives CloudEvents. */
public abstract class CloudEventFunctionHandlerBase<EventT extends Message>
    implements CloudEventsFunction {

  protected static final Logger getLogger(Class<?> javaClass) {
    return LoggerFactory.getLogger(javaClass);
  }

  /**
   * Converts a cloud event into the specific event type.
   *
   * @param cloudEvent The cloud function event.
   * @return The internal event.
   */
  protected abstract EventT toEvent(CloudEvent cloudEvent) throws Exception;

  /**
   * Handles the event.
   *
   * @param event The internal event.
   */
  protected abstract void handleEvent(EventT event) throws Exception;

  /**
   * Accepts an event when invoked by the cloud infrastructure.
   *
   * @param event The cloud event.
   */
  @Override
  public void accept(CloudEvent event) throws Exception {
    handleEvent(toEvent(event));
  }
}
