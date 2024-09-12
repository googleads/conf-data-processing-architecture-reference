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

import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_URL_PATH_OR_VARIABLE;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Key;
import com.google.inject.TypeLiteral;
import com.google.inject.multibindings.MapBinder;
import com.google.scp.shared.api.exception.ServiceException;
import java.util.List;
import java.util.Map;

/** Abstract serverless function class to service HTTP requests. */
public abstract class ServerlessFunction extends AbstractModule {

  /** Locates the {@link ApiTask} that can service the request. */
  protected void invoke(RequestContext request, ResponseContext response) {
    // Create an injector installing itself as the root module.
    Injector injector =
        Guice.createInjector(
            new AbstractModule() {
              @Override
              protected void configure() {
                // Binder that maps base URLs to API tasks that each services an endpoint.
                MapBinder.newMapBinder(
                    binder(), new TypeLiteral<String>() {}, new TypeLiteral<List<ApiTask>>() {});
                install(ServerlessFunction.this);
              }
            });

    Map<String, List<ApiTask>> tasks = injector.getInstance(Key.get(new TypeLiteral<>() {}));
    try {
      dispatch(tasks, request, response);
    } catch (ServiceException exception) {
      response.setError(exception);
    }
  }

  private static void dispatch(
      Map<String, List<ApiTask>> tasks, RequestContext request, ResponseContext response)
      throws ServiceException {
    // Try to find the first task that can handle the request.
    for (String basePath : tasks.keySet()) {
      for (ApiTask task : tasks.get(basePath)) {
        if (task.tryService(basePath, request, response)) {
          return;
        }
      }
    }
    throw new ServiceException(
        NOT_FOUND, INVALID_URL_PATH_OR_VARIABLE.name(), "Resource not found.");
  }
}
