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

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.common.collect.ImmutableList;
import com.google.inject.multibindings.ProvidesIntoMap;
import com.google.inject.multibindings.StringMapKey;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ApiTask;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class AwsServerlessFunctionTest {

  private static final String TEST_RESPONSE = "test-response";

  @Test
  public void testService_happyPath_returnsExpected() throws Exception {
    // Given
    APIGatewayProxyRequestEvent request =
        new APIGatewayProxyRequestEvent().withHttpMethod("GET").withPath("/test-base/path");

    // When
    APIGatewayProxyResponseEvent response = new TestService().handleRequest(request, null);

    // Then
    assertThat(response.getBody()).isEqualTo(TEST_RESPONSE);
  }

  @Test
  public void testService_nonExistingEndpoint_returnsNotFound() throws Exception {
    // Given
    APIGatewayProxyRequestEvent request =
        new APIGatewayProxyRequestEvent().withHttpMethod("GET").withPath("/test-base/no-such");

    // When
    APIGatewayProxyResponseEvent response = new TestService().handleRequest(request, null);

    // Then
    assertThat(response.getStatusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());
  }

  public static final class TestService extends AwsServerlessFunction {
    @ProvidesIntoMap
    @StringMapKey("/test-base")
    List<ApiTask> provideApiTasks() {
      return ImmutableList.of(
          new ApiTask("GET", Pattern.compile("/path")) {
            @Override
            protected void execute(
                Matcher matcher, RequestContext request, ResponseContext response) {
              response.setBody(TEST_RESPONSE);
            }
          });
    }
  }
}
