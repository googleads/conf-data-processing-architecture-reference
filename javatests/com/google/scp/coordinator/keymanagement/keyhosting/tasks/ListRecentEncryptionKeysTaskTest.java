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

package com.google.scp.coordinator.keymanagement.keyhosting.tasks;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.Answers.CALLS_REAL_METHODS;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.google.acai.Acai;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.stream.Stream;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.function.ThrowingRunnable;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class ListRecentEncryptionKeysTaskTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);

  private static final EncryptionKey TEST_KEY = FakeEncryptionKey.create();

  @Inject private InMemoryKeyDb keyDb;
  @Inject private ListRecentEncryptionKeysTask task;

  @Mock private RequestContext request;
  @Mock private Matcher matcher;

  @Mock(answer = CALLS_REAL_METHODS)
  private ResponseContext response;

  @Before
  public void setUp() throws Exception {
    keyDb.createKey(TEST_KEY);
  }

  @Test
  public void execute_happyPath_returnsExpected() throws ServiceException {
    // Given/When
    Stream<EncryptionKey> keys = task.execute(99);

    // Then
    assertThat(keys.count()).isEqualTo(1);
  }

  @Test
  public void execute_negativeAge_throwsException() throws ServiceException {
    // Given/When
    ThrowingRunnable when = () -> task.execute(-2000);

    // Then
    ServiceException exception = assertThrows(ServiceException.class, when);
    assertThat(exception).hasMessageThat().contains("maxAgeSeconds should be positive");
  }

  @Test
  public void testExecute_happyPath_returnsExpected() throws Exception {
    // Given
    doReturn(Optional.of("99")).when(request).getFirstQueryParameter("maxAgeSeconds");

    // When
    task.execute(matcher, request, response);

    // Then
    ArgumentCaptor<String> body = ArgumentCaptor.forClass(String.class);
    verify(response).setBody(body.capture());
    ListRecentEncryptionKeysResponse.Builder keyBuilder =
        ListRecentEncryptionKeysResponse.newBuilder();
    JsonFormat.parser().merge(body.getValue(), keyBuilder);
    ListRecentEncryptionKeysResponse keys = keyBuilder.build();
    assertThat(keys.getKeysList()).isNotEmpty();
  }

  @Test
  public void testExecute_negativeMaxAgeSeconds_throwsException() throws Exception {
    // Given
    doReturn(Optional.of("-123")).when(request).getFirstQueryParameter("maxAgeSeconds");

    // When
    ThrowingRunnable runnable = () -> task.execute(matcher, request, response);

    // Then
    assertThrows(Exception.class, runnable);
  }

  @Test
  public void testExecute_noMaxAgeSeconds_throwsException() throws Exception {
    // Given
    doReturn(Optional.empty()).when(request).getFirstQueryParameter("maxAgeSeconds");

    // When
    ThrowingRunnable runnable = () -> task.execute(matcher, request, response);

    // Then
    assertThrows(Exception.class, runnable);
  }
}
