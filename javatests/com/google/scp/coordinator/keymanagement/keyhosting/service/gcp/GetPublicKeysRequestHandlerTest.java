/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.DATASTORE_ERROR;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.createRandomKey;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.expectedEncodedPublicKey;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.expectedErrorResponseBody;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_HTTP_METHOD;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.startsWith;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.acai.Acai;
import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.util.UUID;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class GetPublicKeysRequestHandlerTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Inject private InMemoryKeyDb keyDb;
  @Inject private KeyService keyService;

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;

  private BufferedWriter writerOut;
  private StringWriter httpResponseOut;
  private GetPublicKeysRequestHandler requestHandler;

  @Before
  public void setUp() throws IOException {
    requestHandler = new GetPublicKeysRequestHandler(keyService);

    httpResponseOut = new StringWriter();
    writerOut = new BufferedWriter(httpResponseOut);
    when(httpResponse.getWriter()).thenReturn(writerOut);
    when(httpRequest.getMethod()).thenReturn("GET");
  }

  @After
  public void reset() {
    keyDb.reset();
  }

  @Test
  public void handleRequest_success() throws IOException {
    EncryptionKey encryptionKey = createRandomKey(keyDb);

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(httpResponseOut.toString(), builder);

    GetActivePublicKeysResponse keysResponse = builder.build();
    assertThat(keysResponse.getKeysList()).hasSize(1);
    assertThat(keysResponse.getKeys(0)).isEqualTo(expectedEncodedPublicKey(encryptionKey));

    // verify header
    verify(httpResponse).appendHeader(eq("Cache-Control"), startsWith("max-age="));
  }

  @Test
  public void service_serviceException() throws IOException {
    String testError = UUID.randomUUID().toString();
    keyDb.setServiceException(new ServiceException(INTERNAL, DATASTORE_ERROR.name(), testError));

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    assertThat(httpResponseOut.toString())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code = */ INTERNAL.getRpcStatusCode(),
                /* message = */ testError,
                /* reason = */ DATASTORE_ERROR.name()));
    verify(httpResponse).setStatusCode(eq(INTERNAL.getHttpStatusCode()));
    // no headers appended
    verify(httpResponse, times(0)).appendHeader(anyString(), anyString());
  }

  @Test
  public void handleRequest_invalidMethod() throws IOException {
    when(httpRequest.getMethod()).thenReturn("POST");

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    assertThat(httpResponseOut.toString())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code = */ INVALID_ARGUMENT.getRpcStatusCode(),
                /* message = */ "Unsupported method: \\u0027POST\\u0027",
                /* reason = */ INVALID_HTTP_METHOD.name()));
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
    // no headers appended
    verify(httpResponse, times(0)).appendHeader(anyString(), anyString());
  }
}
