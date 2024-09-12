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
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.createRandomKey;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.expectedErrorResponseBody;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_HTTP_METHOD;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static org.mockito.ArgumentMatchers.eq;
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
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.util.Optional;
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
public final class ListRecentEncryptionKeysRequestHandlerTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Inject private InMemoryKeyDb keyDb;
  @Inject private KeyService keyService;

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;

  private BufferedWriter writerOut;
  private StringWriter httpResponseOut;
  private ListRecentEncryptionKeysRequestHandler requestHandler;

  @Before
  public void setUp() throws IOException {
    requestHandler = new ListRecentEncryptionKeysRequestHandler(keyService);

    httpResponseOut = new StringWriter();
    writerOut = new BufferedWriter(httpResponseOut);
    when(httpRequest.getMethod()).thenReturn("GET");
    when(httpResponse.getWriter()).thenReturn(writerOut);
  }

  @After
  public void reset() {
    keyDb.reset();
  }

  @Test
  public void handleRequest_success() throws IOException {
    createRandomKey(keyDb);
    EncryptionKey encryptionKey = createRandomKey(keyDb);

    when(httpRequest.getMethod()).thenReturn("GET");
    when(httpRequest.getPath()).thenReturn("/v1alpha/encryptionKeys:recent");
    when(httpRequest.getFirstQueryParameter("maxAgeSeconds")).thenReturn(Optional.of("999"));

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    ListRecentEncryptionKeysResponse.Builder builder =
        ListRecentEncryptionKeysResponse.newBuilder();

    JsonFormat.parser().merge(httpResponseOut.toString(), builder);

    ListRecentEncryptionKeysResponse listKeysResponse = builder.build();
    assertThat(listKeysResponse.getKeysList()).hasSize(2);
    assertThat(listKeysResponse.getKeys(0).getPublicKeyMaterial())
        .isEqualTo(encryptionKey.getPublicKeyMaterial());
    assertThat(listKeysResponse.getKeys(0).getPublicKeysetHandle())
        .isEqualTo(encryptionKey.getPublicKey());
    verify(httpResponse).setStatusCode(eq(200));
  }

  @Test
  public void handleRequest_missingMaxAgeSeconds() throws IOException {
    when(httpRequest.getMethod()).thenReturn("GET");
    when(httpRequest.getPath()).thenReturn("/v1alpha/encryptionKeys:recent");

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    assertThat(httpResponseOut.toString())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code= */ INVALID_ARGUMENT.getRpcStatusCode(),
                /* message= */ "maxAgeSeconds query parameter is required.",
                /* reason= */ INVALID_ARGUMENT.name()));
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
  }

  @Test
  public void handleRequest_invalidMethod() throws IOException {
    when(httpRequest.getMethod()).thenReturn("POST");

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    assertThat(httpResponseOut.toString())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code= */ INVALID_ARGUMENT.getRpcStatusCode(),
                /* message= */ "Unsupported method: \\u0027POST\\u0027",
                /* reason= */ INVALID_HTTP_METHOD.name()));
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
  }
}
