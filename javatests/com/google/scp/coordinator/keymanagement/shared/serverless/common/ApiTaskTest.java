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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class ApiTaskTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock RequestContext request;
  @Mock ResponseContext response;

  @Test
  public void testTryService_matchingRequest_succeedsAndInvokesExecute() throws Exception {
    // Given
    doReturn("test-method").when(request).getMethod();
    doReturn("/test-base/test-path").when(request).getPath();

    // When
    ApiTask task = spy(new TestApiTask("test-method", Pattern.compile("/test-path")));
    boolean success = task.tryService("/test-base", request, response);

    // Then
    assertThat(success).isTrue();
    verify(task).execute(any(), eq(request), eq(response));
  }

  @Test
  public void testTryService_mismatchingMethod_failsAndDoesNotInvokeExecute() throws Exception {
    // Given
    doReturn("wrong-method").when(request).getMethod();
    doReturn("/test-base/test-path").when(request).getPath();

    // When
    ApiTask task = spy(new TestApiTask("test-method", Pattern.compile("/test-path")));
    boolean success = task.tryService("/test-base", request, response);

    // Then
    assertThat(success).isFalse();
    verify(task, never()).execute(any(), any(), any());
  }

  @Test
  public void testTryService_mismatchingPath_failsAndDoesNotInvokeExecute() throws Exception {
    // Given
    doReturn("test-method").when(request).getMethod();
    doReturn("/test-base/wrong-path").when(request).getPath();

    // When
    ApiTask task = spy(new TestApiTask("test-method", Pattern.compile("/test-path")));
    boolean success = task.tryService("/test-base", request, response);

    // Then
    assertThat(success).isFalse();
    verify(task, never()).execute(any(), any(), any());
  }

  @Test
  public void testTryService_mismatchingBase_failsAndDoesNotInvokeExecute() throws Exception {
    // Given
    doReturn("test-method").when(request).getMethod();
    doReturn("/wrong-base/test-path").when(request).getPath();

    // When
    ApiTask task = spy(new TestApiTask("test-method", Pattern.compile("/test-path")));
    boolean success = task.tryService("/test-base", request, response);

    // Then
    assertThat(success).isFalse();
    verify(task, never()).execute(any(), any(), any());
  }

  @Test
  public void testTryService_pathParameter_returnsExpectedMatcher() throws Exception {
    // Given
    doReturn("test-method").when(request).getMethod();
    doReturn("/test-base/test-path/123").when(request).getPath();
    ArgumentCaptor<Matcher> matcher = ArgumentCaptor.forClass(Matcher.class);

    // When
    ApiTask task = spy(new TestApiTask("test-method", Pattern.compile("/test-path/(?<id>.*)")));
    boolean success = task.tryService("/test-base", request, response);

    // Then
    verify(task).execute(matcher.capture(), eq(request), eq(response));
    assertThat(matcher.getValue().group("id")).isEqualTo("123");
  }

  private static class TestApiTask extends ApiTask {
    public TestApiTask(String method, Pattern pattern) {
      super(method, pattern);
    }

    @Override
    protected void execute(Matcher matcher, RequestContext request, ResponseContext response) {}
  }
}
