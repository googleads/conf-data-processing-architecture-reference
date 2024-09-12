/*
 * Copyright 2022 Google LLC
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

package com.google.scp.coordinator.keymanagement.keygeneration.app.common;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.api.model.Code.PERMISSION_DENIED;
import static com.google.scp.shared.api.model.Code.UNKNOWN;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient.KeyStorageServiceException;
import com.google.scp.coordinator.keymanagement.keystorage.converters.EncryptionKeyConverter;
import com.google.scp.coordinator.keymanagement.testutils.FakeDataKeyUtil;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.mapper.TimeObjectMapper;
import java.io.IOException;
import java.net.URI;
import java.util.Optional;
import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.ProtocolVersion;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.entity.StringEntity;
import org.apache.http.message.BasicStatusLine;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

/**
 * Tests the logic (exception handling, parsing, and response validation) in the {@link
 * HttpKeyStorageClient#createKey}.
 */
@RunWith(JUnit4.class)
public final class HttpKeyStorageClientTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpClient httpClient;

  // Under test.
  private HttpKeyStorageClient keyStorageClient;

  private final ObjectMapper mapper = new TimeObjectMapper();

  @Before
  public void setup() {
    keyStorageClient =
        new HttpKeyStorageClient("http://example.com/v1", httpClient, Optional.empty());
  }

  /** Test that the logic in this implementation correctly does not error if a 200 is received */
  @Test
  public void createKey_ok() throws Exception {
    String successResponse = "{}"; // all fields are optional for successful parse
    var response = buildCustomResponse(successResponse, OK.getHttpStatusCode());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    keyStorageClient.createKey(getRequest(), "abc");
  }

  /** Test that a json payload is properly decoded */
  @Test
  public void createKey_decodeJson() throws Exception {
    EncryptionKey payload = getRequest();
    String successResponse =
        JsonFormat.printer().print(EncryptionKeyConverter.toApiEncryptionKey(payload));
    var response = buildCustomResponse(successResponse, OK.getHttpStatusCode());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    EncryptionKey result = keyStorageClient.createKey(payload, "abc");
    // The top-level URI doesn't exist on the API model, so ignore it.
    var payloadNoUri = payload.toBuilder().clearKeyEncryptionKeyUri().clearSetName().build();
    assertThat(result).isEqualTo(payloadNoUri);
  }

  /** Test that an improper json response throws an error as expected */
  @Test
  public void createKey_badJson() throws Exception {
    String successResponse = "true";
    var response = buildCustomResponse(successResponse, OK.getHttpStatusCode());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var exception =
        assertThrows(
            KeyStorageServiceException.class,
            () -> keyStorageClient.createKey(getRequest(), "abc"));

    assertThat(exception).hasCauseThat().isInstanceOf(InvalidProtocolBufferException.class);
  }

  /**
   * Test that the logic in this implementation correctly captures and rethrows {@link
   * KeyStorageServiceException} on HTTP error responses
   */
  @Test
  public void createKey_forbidden() throws Exception {
    var forbiddenResponse = "{\"message\":\"Forbidden\"}";
    var response = buildCustomResponse(forbiddenResponse, PERMISSION_DENIED.getHttpStatusCode());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var exception =
        assertThrows(
            KeyStorageServiceException.class,
            () -> keyStorageClient.createKey(getRequest(), "abc"));

    assertThat(exception).hasCauseThat().isInstanceOf(ServiceException.class);
    ServiceException e = (ServiceException) exception.getCause();
    assertThat(e.getMessage()).contains(forbiddenResponse);
    assertThat(e.getErrorCode()).isEqualTo(UNKNOWN); // Due to absence of code field in JSON
    assertThat(e.getErrorReason()).contains("");
  }

  @Test
  public void createKey_dataKey_success() throws Exception {
    var response = buildCustomResponse("{}", OK.getHttpStatusCode());
    var argument = ArgumentCaptor.forClass(HttpPost.class);
    when(httpClient.execute(any(HttpPost.class))).thenReturn(response);

    keyStorageClient.createKey(getRequest(), FakeDataKeyUtil.createDataKey(), "abc");

    verify(httpClient, times(1)).execute(argument.capture());
    assertThat(argument.getValue().getURI())
        .isEqualTo(new URI("http://example.com/v1/encryptionKeys"));
  }

  @Test
  public void fetchDataKey_success() throws Exception {
    var successResponse = "{\"encryptedDataKey\":\"foo\"}";
    var response = buildCustomResponse(successResponse, OK.getHttpStatusCode());
    var argument = ArgumentCaptor.forClass(HttpPost.class);
    when(httpClient.execute(any(HttpPost.class))).thenReturn(response);

    var dataKey = keyStorageClient.fetchDataKey();

    assertThat(dataKey.getEncryptedDataKey()).isEqualTo("foo");
    verify(httpClient, times(1)).execute(argument.capture());
    assertThat(argument.getValue().getURI())
        .isEqualTo(new URI("http://example.com/v1/encryptionKeys:getDataKey"));
  }

  @Test
  public void fetchDataKey_badJson() throws Exception {
    var responseMessage = "bar";
    var response = buildCustomResponse(responseMessage, OK.getHttpStatusCode());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var exception =
        assertThrows(KeyStorageServiceException.class, () -> keyStorageClient.fetchDataKey());

    assertThat(exception).hasCauseThat().isInstanceOf(InvalidProtocolBufferException.class);
  }

  // API Gateway auth failed message.
  @Test
  public void fetchDataKey_forbidden() throws Exception {
    var forbiddenResponse = "{\"message\":\"Forbidden\"}";
    var response = buildCustomResponse(forbiddenResponse, PERMISSION_DENIED.getHttpStatusCode());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var exception =
        assertThrows(KeyStorageServiceException.class, () -> keyStorageClient.fetchDataKey());

    assertThat(exception).hasCauseThat().isInstanceOf(ServiceException.class);
    ServiceException e = (ServiceException) exception.getCause();
    assertThat(e.getMessage()).contains(forbiddenResponse);
    assertThat(e.getErrorCode()).isEqualTo(UNKNOWN); // Due to absence of code field in JSON
    assertThat(e.getErrorReason()).contains("");
  }

  @Test
  public void fetchDataKey_respectsOverride() throws Exception {
    keyStorageClient =
        new HttpKeyStorageClient(
            "http://example.com/v1", httpClient, Optional.of("http://override.example.com/v1"));
    var argument = ArgumentCaptor.forClass(HttpUriRequest.class);
    var response = buildCustomResponse("{}", OK.getHttpStatusCode());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    keyStorageClient.createKey(getRequest(), "foo");
    keyStorageClient.fetchDataKey();

    verify(httpClient, times(2)).execute(argument.capture());
    assertThat(argument.getAllValues().size()).isEqualTo(2);
    assertThat(argument.getAllValues().get(0).getURI().toString())
        .isEqualTo("http://example.com/v1/encryptionKeys");
    assertThat(argument.getAllValues().get(1).getURI().toString())
        .isEqualTo("http://override.example.com/v1/encryptionKeys:getDataKey");
  }

  private HttpResponse buildCustomResponse(String response, int statusCode) throws IOException {
    HttpResponse httpResponse = Mockito.mock(HttpResponse.class);
    HttpEntity httpEntity = new StringEntity(response);
    when(httpResponse.getEntity()).thenReturn(httpEntity);
    var protocol = new ProtocolVersion("HTTP", 1, 1);
    when(httpResponse.getStatusLine()).thenReturn(new BasicStatusLine(protocol, statusCode, ""));
    return httpResponse;
  }

  private EncryptionKey getRequest() {
    return FakeEncryptionKey.create().toBuilder().setJsonEncodedKeyset("").build();
  }
}
