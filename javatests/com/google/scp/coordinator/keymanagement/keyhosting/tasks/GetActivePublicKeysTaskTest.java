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

package com.google.scp.coordinator.keymanagement.keyhosting.tasks;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.addRandomKeysToKeyDb;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static org.junit.Assert.assertThrows;
import static org.mockito.Answers.CALLS_REAL_METHODS;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.google.acai.Acai;
import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey.KeyOneofCase;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
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
  public void getActivePublicKeys_hasAllPublicKeys() throws ServiceException {
    ImmutableList<EncryptionKey> publicKeys = task.getActivePublicKeys();

    assertThat(publicKeys).hasSize(keyLimit);
  }

  @Test
  public void getActivePublicKeys_errorResponse() {
    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));

    assertThrows(ServiceException.class, () -> task.getActivePublicKeys());
  }

  @Test
  public void testExecute_defaultRequest_returnsUpToKeyLimit() throws Exception {
    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    assertThat(keys.getKeysList()).hasSize(keyLimit);
  }

  @Test
  public void testExecute_defaultRequest_returnsExpectedFormat() throws Exception {
    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.KEY);
  }

  @Test
  public void testExecute_defaultRequest_returnsExpectedCorsHeader() throws Exception {
    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);

    List<String> values = getHeaders(response).get("Access-Control-Allow-Origin");
    assertThat(values).containsExactly("*");
  }

  @Test
  public void testExecute_defaultRequest_returnsExpectedCacheControl() throws Exception {
    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    ImmutableSet<String> ids =
        keys.getKeysList().stream()
            .map(EncodedPublicKey::getId)
            .collect(ImmutableSet.toImmutableSet());
    Long secondsUntilFirstExpiration =
        keyDb.getAllKeys().stream()
            .filter(key -> ids.contains(key.getKeyId()))
            .map(EncryptionKey::getExpirationTime)
            .min(Long::compareTo)
            .map(soonest -> Instant.now().until(Instant.ofEpochMilli(soonest), ChronoUnit.SECONDS))
            .get();

    List<String> values = getHeaders(response).get("Cache-Control");
    assertThat(values).hasSize(1);

    Matcher matcher = Pattern.compile("max-age=(\\d+)").matcher(values.get(0));
    assertThat(matcher.find()).isTrue();
    long cacheMaxAge = Long.parseLong(matcher.group(1));

    // Account for time elapsed since the task computed it.
    assertThat(cacheMaxAge).isAtMost(secondsUntilFirstExpiration);
    assertThat(cacheMaxAge).isWithin(5).of(secondsUntilFirstExpiration);
  }

  @Test
  public void testExecute_noKeys_returnsNoCacheControl() throws Exception {
    // Given
    keyDb.reset();

    // When
    task.execute(matcher, request, response);

    // Then
    assertThat(verifyResponse(response).getKeysList()).isEmpty();
    assertThat(getHeaders(response).get("Cache-Control")).isEmpty();
  }

  @Test
  public void testExecute_rawRequest_returnsExpectedFormat() throws Exception {
    // Given
    doReturn(":raw").when(matcher).group("raw");

    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.HPKE_PUBLIC_KEY);
  }

  @Test
  public void testExecute_tinkRequest_returnsExpectedFormat() throws Exception {
    // Given
    doReturn(":tink").when(matcher).group("tink");

    // When
    task.execute(matcher, request, response);

    // Then
    GetActivePublicKeysResponse keys = verifyResponse(response);
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.TINK_BINARY);
  }

  private static GetActivePublicKeysResponse verifyResponse(ResponseContext response)
      throws Exception {
    ArgumentCaptor<String> body = ArgumentCaptor.forClass(String.class);
    verify(response).setBody(body.capture());
    GetActivePublicKeysResponse.Builder keysBuilder = GetActivePublicKeysResponse.newBuilder();

    JsonFormat.parser().merge(body.getValue(), keysBuilder);
    return keysBuilder.build();
  }

  private static ArrayListMultimap<String, String> getHeaders(ResponseContext response)
      throws Exception {
    ArgumentCaptor<String> key = ArgumentCaptor.forClass(String.class);
    ArgumentCaptor<String> value = ArgumentCaptor.forClass(String.class);
    verify(response, atLeast(0)).addHeader(key.capture(), value.capture());
    GetActivePublicKeysResponse.Builder keysBuilder = GetActivePublicKeysResponse.newBuilder();

    ArrayListMultimap<String, String> headers = ArrayListMultimap.create();

    for (int i = 0; i < key.getAllValues().size(); i++) {
      headers.put(key.getAllValues().get(i), value.getAllValues().get(i));
    }
    return headers;
  }
}
