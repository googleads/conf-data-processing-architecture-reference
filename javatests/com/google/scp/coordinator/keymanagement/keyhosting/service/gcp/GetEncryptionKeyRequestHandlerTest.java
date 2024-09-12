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
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.expectedErrorResponseBody;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_HTTP_METHOD;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_URL_PATH_OR_VARIABLE;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static java.util.UUID.randomUUID;
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
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class GetEncryptionKeyRequestHandlerTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Inject private InMemoryKeyDb keyDb;
  @Inject private KeyService keyService;

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;

  private BufferedWriter writerOut;
  private StringWriter httpResponseOut;
  private GetEncryptionKeyRequestHandler requestHandler;

  @Before
  public void setUp() throws IOException {
    requestHandler = new GetEncryptionKeyRequestHandler(keyService);

    httpResponseOut = new StringWriter();
    writerOut = new BufferedWriter(httpResponseOut);
    when(httpRequest.getMethod()).thenReturn("GET");
    when(httpRequest.getUri()).thenReturn("cloudfunctions");
    when(httpResponse.getWriter()).thenReturn(writerOut);
  }

  @Test
  public void handleRequest_success() throws IOException {
    EncryptionKey encryptionKey = createRandomKey(keyDb);
    String keyId = encryptionKey.getKeyId();
    when(httpRequest.getPath()).thenReturn("/v1alpha/encryptionKeys/" + keyId);

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey
            .Builder
        builder =
            com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto
                .EncryptionKey.newBuilder();

    JsonFormat.parser().merge(httpResponseOut.toString(), builder);
    com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey
        resEncryptionKey = builder.build();

    assertThat(resEncryptionKey.getPublicKeysetHandle()).isEqualTo(encryptionKey.getPublicKey());
    assertThat(resEncryptionKey.getKeyDataList().get(0).getKeyMaterial())
        .isEqualTo(encryptionKey.getJsonEncodedKeyset());
    verify(httpResponse).setStatusCode(eq(200));
  }

  @Test
  public void handleRequest_keyNotFound() throws IOException {
    String testError = UUID.randomUUID().toString();
    keyDb.setServiceException(new ServiceException(NOT_FOUND, DATASTORE_ERROR.name(), testError));
    when(httpRequest.getPath()).thenReturn("/v1alpha/encryptionKeys/" + randomUUID());

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    assertThat(httpResponseOut.toString())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code = */ NOT_FOUND.getRpcStatusCode(),
                /* message = */ testError,
                /* reason = */ DATASTORE_ERROR.name()));
    verify(httpResponse).setStatusCode(eq(404));
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
  }

  @Test
  public void handleRequest_invalidPath() throws IOException {
    String invalidPath = "/" + randomUUID() + "/errorSuffix";
    assertInvalidPath(invalidPath);
  }

  private void assertInvalidPath(String invalidPath) throws IOException {
    when(httpRequest.getPath()).thenReturn(invalidPath);

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    assertThat(httpResponseOut.toString())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code = */ INVALID_ARGUMENT.getRpcStatusCode(),
                /* message = */ "Invalid URL Path: \\u0027" + invalidPath + "\\u0027",
                /* reason = */ INVALID_URL_PATH_OR_VARIABLE.name()));
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
  }
}
