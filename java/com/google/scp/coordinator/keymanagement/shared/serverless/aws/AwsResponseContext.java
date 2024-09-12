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

package com.google.scp.coordinator.keymanagement.shared.serverless.aws;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import java.util.HashMap;

/** {@link ResponseContext} implementation for AWS Lambda. */
public class AwsResponseContext extends ResponseContext {

  private final APIGatewayProxyResponseEvent event;

  public AwsResponseContext(APIGatewayProxyResponseEvent event) {
    this.event = event;
    event.setHeaders(new HashMap<>());
  }

  public void setBody(String body) {
    event.setBody(body);
  }

  @Override
  public void addHeader(String name, String value) {
    event.getHeaders().put(name, value);
  }

  public void setStatusCode(int i) {
    event.setStatusCode(i);
  }
}
