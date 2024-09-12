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

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;

import com.google.cloud.functions.invoker.runner.Invoker;
import com.google.common.collect.ImmutableList;
import com.google.inject.multibindings.ProvidesIntoMap;
import com.google.inject.multibindings.StringMapKey;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ApiTask;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import java.net.ServerSocket;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.apache.http.HttpResponse;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class GcpServerlessFunctionTest {

  private static final String TEST_RESPONSE = "test-response";

  private static int testPort;

  private static Invoker invoker;

  @BeforeClass
  public static void setUp() throws Exception {
    // Find an available port.
    try (ServerSocket socket = new ServerSocket(0)) {
      socket.setReuseAddress(true);
      testPort = socket.getLocalPort();
    }
    // Start the HTTP server for the test service.
    invoker =
        new Invoker(
            testPort,
            TestService.class.getCanonicalName(),
            "http",
            Thread.currentThread().getContextClassLoader());
    invoker.startTestServer();
  }

  @AfterClass
  public static void tearDown() throws Exception {
    invoker.stopServer();
  }

  @Test
  public void testService_happyPath_returnsExpected() throws Exception {
    // Given
    HttpGet request = new HttpGet(String.format("http://localhost:%d/test-base/path", testPort));

    // When
    String body;
    try (CloseableHttpClient client = HttpClients.createDefault()) {
      HttpResponse response = client.execute(request);
      body = EntityUtils.toString(response.getEntity());
    }

    // Then
    assertThat(body).isEqualTo(TEST_RESPONSE);
  }

  @Test
  public void testService_nonExistingEndpoint_returnsNotFound() throws Exception {
    // Given
    HttpGet request = new HttpGet(String.format("http://localhost:%d/test-base/no-such", testPort));

    // When
    HttpResponse response;
    try (CloseableHttpClient client = HttpClients.createDefault()) {
      response = client.execute(request);
    }

    // Then
    assertThat(response.getStatusLine().getStatusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());
  }

  public static final class TestService extends GcpServerlessFunction {
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
