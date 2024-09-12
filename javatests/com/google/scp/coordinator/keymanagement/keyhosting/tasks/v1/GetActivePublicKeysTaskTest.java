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

package com.google.scp.coordinator.keymanagement.keyhosting.tasks.v1;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb.DEFAULT_SET_NAME;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.addRandomKeysToKeyDb;
import static org.mockito.Answers.CALLS_REAL_METHODS;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.google.acai.Acai;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey.KeyOneofCase;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import java.util.regex.Matcher;
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
public class GetActivePublicKeysTaskTest {

  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Inject private InMemoryKeyDb keyDb;
  @Inject private GetActivePublicKeysTask task;
  @Inject @KeyLimit private Integer keyLimit;

  @Mock private RequestContext request;
  @Mock private Matcher matcher;

  @Mock(answer = CALLS_REAL_METHODS)
  private ResponseContext response;

  @Before
  public void setUp() {
    addRandomKeysToKeyDb(keyLimit * 2, keyDb);
  }

  @Test
  public void testExecute_defaultSet_returnsUpToKeyLimit() throws Exception {
    // Given
    doReturn(DEFAULT_SET_NAME).when(matcher).group("name");

    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    assertThat(keys.getKeysList()).hasSize(keyLimit);
  }

  @Test
  public void testExecute_specificSetWithoutKeys_returnsEmpty() throws Exception {
    // Given
    doReturn("set-with-no-keys").when(matcher).group("name");

    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    assertThat(keys.getKeysList()).isEmpty();
  }

  @Test
  public void testExecute_defaultSet_returnsExpectedFormat() throws Exception {
    // Given
    doReturn(DEFAULT_SET_NAME).when(matcher).group("name");

    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.TINK_BINARY);
  }

  @Test
  public void testExecute_defaultSetInRaw_returnsExpectedFormat() throws Exception {
    // Given
    doReturn(DEFAULT_SET_NAME).when(matcher).group("name");
    doReturn(":raw").when(matcher).group("raw");

    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.HPKE_PUBLIC_KEY);
  }

  private static GetActivePublicKeysResponse verifyResponse(ResponseContext response)
      throws Exception {
    ArgumentCaptor<String> body = ArgumentCaptor.forClass(String.class);
    verify(response).setBody(body.capture());
    GetActivePublicKeysResponse.Builder keysBuilder = GetActivePublicKeysResponse.newBuilder();

    JsonFormat.parser().merge(body.getValue(), keysBuilder);
    return keysBuilder.build();
  }
}
