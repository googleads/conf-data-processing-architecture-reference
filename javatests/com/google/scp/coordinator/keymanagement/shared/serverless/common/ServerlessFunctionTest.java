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

import static org.mockito.Answers.CALLS_REAL_METHODS;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import com.google.common.collect.ImmutableList;
import com.google.inject.multibindings.ProvidesIntoMap;
import com.google.inject.multibindings.StringMapKey;
import com.google.scp.shared.api.model.Code;
import java.util.List;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class ServerlessFunctionTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock RequestContext testRequest;
  @Mock ApiTask testTask0;
  @Mock ApiTask testTask1;

  @Mock(answer = CALLS_REAL_METHODS)
  ResponseContext testResponse;

  @Test
  public void testInvoke_matchingApiTask_noNotFoundResponse() throws Exception {
    // Given
    doReturn(true).when(testTask0).tryService(any(), any(), any());
    ServerlessFunction serverless = createServerlessFunction(testTask0);

    // When
    serverless.invoke(testRequest, testResponse);

    // Then
    verify(testResponse, never()).setError(any());
  }

  @Test
  public void testInvoke_atLeastOneMatchingTaskInMany_noNotFoundResponse() throws Exception {
    // Given
    doReturn(false).when(testTask0).tryService(any(), any(), any());
    doReturn(true).when(testTask1).tryService(any(), any(), any());
    ServerlessFunction serverless = createServerlessFunction(testTask0, testTask1);

    // When
    serverless.invoke(testRequest, testResponse);

    // Then
    verify(testResponse, never()).setError(any());
  }

  @Test
  public void testInvoke_multipleMatchingApiTasks_subsequentOneNeverTried() throws Exception {
    // Given
    doReturn(true).when(testTask0).tryService(any(), any(), any());
    doReturn(true).when(testTask1).tryService(any(), any(), any());
    ServerlessFunction serverless = createServerlessFunction(testTask0, testTask1);

    // When
    serverless.invoke(testRequest, testResponse);

    // Then
    verify(testTask1, never()).tryService(any(), any(), any());
  }

  @Test
  public void testInvoke_noTask_notFoundResponse() {
    // Given
    ServerlessFunction serverless = createServerlessFunction();

    // When
    serverless.invoke(testRequest, testResponse);

    // Then
    verify(testResponse).setStatusCode(Code.NOT_FOUND.getHttpStatusCode());
  }

  @Test
  public void testInvoke_noMatchingTask_notFoundResponse() throws Exception {
    // Given
    doReturn(false).when(testTask0).tryService(any(), any(), any());
    doReturn(false).when(testTask1).tryService(any(), any(), any());
    ServerlessFunction serverless = createServerlessFunction(testTask0, testTask1);

    // When
    serverless.invoke(testRequest, testResponse);

    // Then
    verify(testResponse).setStatusCode(Code.NOT_FOUND.getHttpStatusCode());
  }

  private static ServerlessFunction createServerlessFunction(ApiTask... apiTasks) {
    return new ServerlessFunction() {
      @ProvidesIntoMap
      @StringMapKey("/v123")
      List<ApiTask> provideApiTasks() {
        return ImmutableList.copyOf(apiTasks);
      }
    };
  }
}
