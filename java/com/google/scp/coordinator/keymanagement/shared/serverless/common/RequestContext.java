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

package com.google.scp.coordinator.keymanagement.shared.serverless.common;

import java.util.Optional;

/** Represents the HTTP request. */
public abstract class RequestContext {

  /* Returns the request path. */
  public abstract String getPath();

  /* Returns the request method. */
  public abstract String getMethod();

  /* Returns the first query parameter found for the given parameter name. */
  public abstract Optional<String> getFirstQueryParameter(String name);
}
