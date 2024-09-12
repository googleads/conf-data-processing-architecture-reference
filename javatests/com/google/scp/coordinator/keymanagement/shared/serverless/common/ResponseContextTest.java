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
package com.google.scp.coordinator.keymanagement.shared.serverless.common;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.verify;

import com.google.scp.coordinator.keymanagement.shared.serverless.common.CommonProto.TestMessage;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Answers;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class ResponseContextTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock(answer = Answers.CALLS_REAL_METHODS)
  ResponseContext response;

  @Test
  public void testSetBody_protoMessage_setsExpectedHeaderAndBody() {
    // Given
    TestMessage message = TestMessage.newBuilder().setValue("test-value").build();

    // When
    response.setBody(message);

    // Then
    verify(response).addHeader("Content-Type", "application/json");

    ArgumentCaptor<String> body = ArgumentCaptor.forClass(String.class);
    verify(response).setBody(body.capture());
    assertThat(body.getValue()).isEqualTo("{\"value\":\"test-value\"}");
  }

  @Test
  public void testSetError_ServiceException_setsExpectedStatusAndError() {
    // Given
    ServiceException exception =
        new ServiceException(
            Code.INTERNAL, "test-error", "test-message", new IllegalArgumentException());

    // When
    response.setError(exception);

    // Then
    verify(response).setStatusCode(exception.getErrorCode().getHttpStatusCode());

    ArgumentCaptor<String> body = ArgumentCaptor.forClass(String.class);
    verify(response).setBody(body.capture());
    assertThat(body.getValue())
        .isEqualTo(
            "{\"code\":13,\"message\":\"test-message\",\"details\":[{\"reason\":\"test-error\",\"domain\":\"\",\"metadata\":{}}]}");
  }
}
