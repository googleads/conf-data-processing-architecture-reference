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

import com.google.scp.shared.api.exception.ServiceException;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** An API task to handle requests to a specific endpoint. */
public abstract class ApiTask {
  private final String method;
  private final Pattern path;

  protected ApiTask(String method, Pattern path) {
    this.method = method;
    this.path = path;
  }

  /** Executes teh task for the matched request. */
  protected abstract void execute(Matcher matcher, RequestContext request, ResponseContext response)
      throws ServiceException;

  /**
   * Services the request if it matches the task, otherwise returns false.
   *
   * @param basePath the base URL path.
   */
  boolean tryService(String basePath, RequestContext request, ResponseContext response)
      throws ServiceException {
    if (!Objects.equals(request.getMethod(), method) || !request.getPath().startsWith(basePath)) {
      return false;
    }
    String subPath = request.getPath().substring(basePath.length());
    Matcher matcher = path.matcher(subPath);
    if (!matcher.matches()) {
      return false;
    }
    execute(matcher, request, response);
    return true;
  }
}
