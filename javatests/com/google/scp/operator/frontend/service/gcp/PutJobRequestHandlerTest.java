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

package com.google.scp.operator.frontend.service.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_HTTP_METHOD;
import static com.google.scp.shared.api.model.Code.ACCEPTED;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.cmrt.sdk.job_service.v1.PutJobRequest;
import com.google.cmrt.sdk.job_service.v1.PutJobResponse;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.frontend.service.FrontendService;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.shared.dao.metadatadb.testing.HelloWorld;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringReader;
import java.io.StringWriter;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class PutJobRequestHandlerTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;
  @Mock private FrontendService service;

  private static final String jobId = "1234";
  private static final String name = "JohnDoe";
  private static final Integer id = 6678;

  private StringWriter httpResponseOut;
  private BufferedWriter writerOut;
  private PutJobRequestHandler requestHandler;

  @Before
  public void setUp() throws IOException {
    requestHandler = new PutJobRequestHandler(service);
    httpResponseOut = new StringWriter();
    writerOut = new BufferedWriter(httpResponseOut);
    when(httpResponse.getWriter()).thenReturn(writerOut);
  }

  @Test
  public void handleRequest_success() throws Exception {
    setRequestBody(getValidFakeRequestJson());

    when(httpRequest.getMethod()).thenReturn("POST");
    when(service.putJob(any())).thenReturn(PutJobResponse.getDefaultInstance());

    requestHandler.handleRequest(httpRequest, httpResponse);

    writerOut.flush();
    verify(httpResponse).setStatusCode(eq(ACCEPTED.getHttpStatusCode()));

    ArgumentCaptor<PutJobRequest> argument = ArgumentCaptor.forClass(PutJobRequest.class);
    verify(service).putJob(argument.capture());
    assertEquals(jobId, argument.getValue().getJobId());
    assertEquals(getValidJobBody(), argument.getValue().getJobBody());
  }

  @Test
  public void handleRequest_invalidJson() throws Exception {
    String jsonRequest = "asdf}{";
    setRequestBody(jsonRequest);
    when(httpRequest.getMethod()).thenReturn("POST");

    requestHandler.handleRequest(httpRequest, httpResponse);

    writerOut.flush();
    String response = httpResponseOut.toString();
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
    assertThat(response).contains("\"code\": " + INVALID_ARGUMENT.getRpcStatusCode());
    assertThat(response).contains("Expect message object but got: \\\"asdf\\\"");
    assertThat(response).contains("\"reason\": \"" + ErrorReasons.JSON_ERROR.name());
  }

  @Test
  public void handleRequest_invalidHttpMethod() throws Exception {
    setRequestBody(getValidFakeRequestJson());
    when(httpRequest.getMethod()).thenReturn("PUT");

    requestHandler.handleRequest(httpRequest, httpResponse);

    writerOut.flush();
    String response = httpResponseOut.toString();
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
    assertThat(response).contains("\"code\": " + INVALID_ARGUMENT.getRpcStatusCode());
    assertThat(response).contains("\"message\": \"Unsupported method: \\u0027PUT\\u0027\"");
    assertThat(response).contains("\"reason\": \"" + INVALID_HTTP_METHOD.name());
  }

  private String getValidJobBody() throws InvalidProtocolBufferException {
    HelloWorld helloworld = HelloWorld.newBuilder().setName(name).setId(id).build();
    return new String(helloworld.toByteArray(), UTF_8);
  }

  private String getValidFakeRequestJson() throws InvalidProtocolBufferException {
    var jobBody = getValidJobBody();
    PutJobRequest request = PutJobRequest.newBuilder().setJobId(jobId).setJobBody(jobBody).build();
    return JsonFormat.printer().print(request);
  }

  private void setRequestBody(String body) throws IOException {
    BufferedReader reader = new BufferedReader(new StringReader(body));
    when(httpRequest.getReader()).thenReturn(reader);
  }
}
