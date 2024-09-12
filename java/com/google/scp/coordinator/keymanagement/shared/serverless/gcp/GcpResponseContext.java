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

package com.google.scp.coordinator.keymanagement.shared.serverless.gcp;

import com.google.cloud.functions.HttpResponse;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import java.io.IOException;
import java.io.UncheckedIOException;

/** {@link ResponseContext} implementation for GCP Cloud Functions. */
public class GcpResponseContext extends ResponseContext {

  private final HttpResponse response;

  public GcpResponseContext(HttpResponse response) {
    this.response = response;
  }

  public void setBody(String body) {
    try {
      response.getWriter().write(body);
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  @Override
  public void addHeader(String name, String value) {
    response.appendHeader(name, value);
  }

  @Override
  public void setStatusCode(int i) {
    response.setStatusCode(i);
  }
}
