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

package com.google.scp.coordinator.keymanagement.keyhosting.service.aws;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.DYNAMODB_ERROR;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.createRandomKey;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.expectedEncodedPublicKey;
import static com.google.scp.shared.api.model.Code.INTERNAL;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.google.acai.Acai;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.util.UUID;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class PublicKeyApiGatewayHandlerTest {

  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Inject private InMemoryKeyDb keyDb;
  @Inject private KeyService keyService;
  private PublicKeyApiGatewayHandler lambda;

  @Before
  public void setUp() {
    lambda = new PublicKeyApiGatewayHandler(keyService);
  }

  @After
  public void reset() {
    keyDb.reset();
  }

  @Test
  public void handleRequest() throws JsonProcessingException, InvalidProtocolBufferException {
    EncryptionKey encryptionKey = createRandomKey(keyDb);

    APIGatewayProxyResponseEvent response = lambda.handleRequest(null, null);

    assertThat(response).isNotNull();
    assertThat(response.getStatusCode()).isEqualTo(200);

    // verify key
    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(response.getBody(), builder);
    GetActivePublicKeysResponse keysResponse = builder.build();
    assertThat(keysResponse.getKeysList()).hasSize(1);
    assertThat(keysResponse.getKeysList().get(0))
        .isEqualTo(expectedEncodedPublicKey(encryptionKey));

    // verify header
    assertThat(response.getHeaders()).isNotEmpty();
    assertThat(response.getHeaders()).containsKey("Cache-Control");
    assertThat(response.getHeaders().get("Cache-Control")).startsWith("max-age=");
  }

  @Test
  public void handleRequest_withNoKeys() throws InvalidProtocolBufferException {
    APIGatewayProxyResponseEvent response = lambda.handleRequest(null, null);

    assertThat(response).isNotNull();
    assertThat(response.getStatusCode()).isEqualTo(200);

    // verify key
    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(response.getBody(), builder);
    GetActivePublicKeysResponse keysResponse = builder.build();
    assertThat(keysResponse.getKeysList()).hasSize(0);

    // verify header
    assertThat(response.getHeaders()).isNotEmpty();
    assertThat(response.getHeaders()).doesNotContainKey("Cache-Control");
  }

  @Test
  public void handleRequest_serviceException() {
    String testError = UUID.randomUUID().toString();
    keyDb.setServiceException(new ServiceException(INTERNAL, DYNAMODB_ERROR.name(), testError));

    APIGatewayProxyResponseEvent response = lambda.handleRequest(null, null);

    assertThat(response).isNotNull();
    assertThat(response.getStatusCode()).isEqualTo(INTERNAL.getHttpStatusCode());
    assertThat(response.getBody()).isNotNull();
    assertThat(response.getBody()).contains(String.valueOf(INTERNAL.getRpcStatusCode()));
    assertThat(response.getBody()).contains(DYNAMODB_ERROR.name());
    assertThat(response.getBody()).contains(testError);
  }
}
