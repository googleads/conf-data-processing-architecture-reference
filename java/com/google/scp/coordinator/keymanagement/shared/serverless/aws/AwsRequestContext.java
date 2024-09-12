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

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import java.util.Optional;

/** {@link RequestContext} implementation for AWS Lambda. */
public class AwsRequestContext extends RequestContext {

  private final APIGatewayProxyRequestEvent event;

  @Inject
  public AwsRequestContext(APIGatewayProxyRequestEvent event) {
    this.event = event;
  }

  public String getPath() {
    return event.getPath();
  }

  public String getMethod() {
    return event.getHttpMethod();
  }

  public Optional<String> getFirstQueryParameter(String name) {
    return Optional.ofNullable(event.getQueryStringParameters().getOrDefault(name, null));
  }
}
