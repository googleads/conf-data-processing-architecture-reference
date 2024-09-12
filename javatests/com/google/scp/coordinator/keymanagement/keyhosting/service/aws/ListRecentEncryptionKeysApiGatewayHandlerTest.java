/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.keymanagement.keyhosting.service.aws;

import static com.google.common.truth.Truth.assertThat;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.acai.Acai;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.model.Code;
import java.util.stream.Stream;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class ListRecentEncryptionKeysApiGatewayHandlerTest {

  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Inject private InMemoryKeyDb keyDb;
  @Inject private KeyService keyService;
  @Inject private ListRecentEncryptionKeysApiGatewayHandler handler;

  @Test
  public void handleRequest_happyPath_returnsExpected() throws Exception {
    // Given
    keyDb.createKey(FakeEncryptionKey.create());
    keyDb.createKey(FakeEncryptionKey.create());

    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setPath("/encryptionKeys:recent");
    request.setQueryStringParameters(ImmutableMap.of("maxAgeSeconds", "9"));

    // When
    APIGatewayProxyResponseEvent event = handler.handleRequest(request, null);
    ListRecentEncryptionKeysResponse.Builder builder =
        ListRecentEncryptionKeysResponse.newBuilder();
    JsonFormat.parser().merge(event.getBody(), builder);
    ListRecentEncryptionKeysResponse response = builder.build();

    // Then
    assertThat(event.getStatusCode()).isEqualTo(200);
    assertThat(response.getKeysList()).hasSize(2);
  }

  @Test
  public void handleRequest_arbitraryPathPrefixes_returnsExpected() {
    // Given
    Stream<String> paths =
        Stream.of(
            "/encryptionKeys:recent",
            "/abc/encryptionKeys:recent",
            "/abc/efg/encryptionKeys:recent",
            "/abc/efg/hij/encryptionKeys:recent");

    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setQueryStringParameters(ImmutableMap.of("maxAgeSeconds", "9"));

    // When
    Stream<APIGatewayProxyResponseEvent> events =
        paths.map(
            path -> {
              request.setPath(path);
              return handler.handleRequest(request, null);
            });

    // Then
    events.forEach(
        event -> {
          assertThat(event.getStatusCode()).isEqualTo(200);
        });
  }

  @Test
  public void handleRequest_invalidPaths_returnsExpectedErrors() {
    // Given
    Stream<String> paths =
        Stream.of(
            "/encryptionKeys:recentxxxx",
            "xxxx/encryptionKeys:recentjunk",
            "/encryptionKeys",
            "/encryptionKeys/recent",
            "/encryptionKeysrecent");

    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setQueryStringParameters(ImmutableMap.of("maxAgeSeconds", "9"));

    // When
    Stream<APIGatewayProxyResponseEvent> events =
        paths.map(
            path -> {
              request.setPath(path);
              return handler.handleRequest(request, null);
            });

    // Then
    events.forEach(
        event -> {
          assertThat(event.getStatusCode()).isEqualTo(400);
          ErrorResponse error = parseBody(event, ErrorResponse.newBuilder()).build();
          assertThat(error.getCode()).isEqualTo(Code.INVALID_ARGUMENT.getRpcStatusCode());
          assertThat(error.getMessage()).contains("Invalid URL Path");
        });
  }

  @Test
  public void handleRequest_noQueryString_returnsExpectedErrors() {
    // Given
    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setPath("/encryptionKeys:recent");

    // When
    APIGatewayProxyResponseEvent event = handler.handleRequest(request, null);

    // Then
    assertThat(event.getStatusCode()).isEqualTo(400);
    ErrorResponse error = parseBody(event, ErrorResponse.newBuilder()).build();
    assertThat(error.getCode()).isEqualTo(Code.INVALID_ARGUMENT.getRpcStatusCode());
    assertThat(error.getMessage()).contains("maxAgeSeconds is required");
  }

  @Test
  public void handleRequest_invalidMaxAgeSeconds_returnsExpectedErrors() {
    // Given
    Stream<String> maxAgeSecondsValues = Stream.of("", "0x123", "999999999999999999999");

    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setPath("/encryptionKeys:recent");

    // When
    Stream<APIGatewayProxyResponseEvent> events =
        maxAgeSecondsValues.map(
            value -> {
              request.setQueryStringParameters(ImmutableMap.of("maxAgeSeconds", value));
              return handler.handleRequest(request, null);
            });

    // Then
    events.forEach(
        event -> {
          assertThat(event.getStatusCode()).isEqualTo(400);
          ErrorResponse error = parseBody(event, ErrorResponse.newBuilder()).build();
          assertThat(error.getCode()).isEqualTo(Code.INVALID_ARGUMENT.getRpcStatusCode());
          assertThat(error.getMessage()).contains("maxAgeSeconds should be a valid integer.");
        });
  }

  private <T extends Message.Builder> T parseBody(
      APIGatewayProxyResponseEvent response, T builder) {
    try {
      JsonFormat.parser().merge(response.getBody(), builder);
    } catch (InvalidProtocolBufferException e) {
      throw new RuntimeException(e);
    }
    return builder;
  }
}
