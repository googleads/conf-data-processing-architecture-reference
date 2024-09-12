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

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.io.IOException;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class GetEncryptionKeyApiGatewayHandlerTest {

  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Inject private InMemoryKeyDb keyDb;
  @Inject private KeyService keyService;
  private GetEncryptionKeyApiGatewayHandler lambda;

  @Before
  public void getInstances() {
    lambda = new GetEncryptionKeyApiGatewayHandler(keyService, false);
  }

  @Test
  public void handleRequest_handlesValidResponse() throws IOException, ServiceException {
    String keyId = "abcd";
    putPrivateKey(keyId);
    APIGatewayProxyRequestEvent req = new APIGatewayProxyRequestEvent();
    req.setPath("/v1/encryptionKeys/" + keyId);

    APIGatewayProxyResponseEvent response = lambda.handleRequest(req, null);

    assertThat(response.getStatusCode()).isEqualTo(200);

    EncryptionKey.Builder builder = EncryptionKey.newBuilder();
    JsonFormat.parser().merge(response.getBody(), builder);
    EncryptionKey encryptionKey = builder.build();
    assertThat(encryptionKey.getName()).isEqualTo("encryptionKeys/abcd");
    assertThat(encryptionKey.getKeyDataList().get(0).getKeyMaterial()).isEqualTo("{test}");
  }

  @Test
  public void handleRequest_handlesAnyPath() throws IOException, ServiceException {
    String keyId = "abcd";
    putPrivateKey(keyId);
    APIGatewayProxyRequestEvent req = new APIGatewayProxyRequestEvent();
    req.setPath("/stage/v1/encryptionKeys/" + keyId);

    APIGatewayProxyResponseEvent response = lambda.handleRequest(req, null);

    assertThat(response.getStatusCode()).isEqualTo(200);

    EncryptionKey.Builder builder = EncryptionKey.newBuilder();
    JsonFormat.parser().merge(response.getBody(), builder);
    EncryptionKey encryptionKey = builder.build();
    assertThat(encryptionKey.getName()).isEqualTo("encryptionKeys/abcd");
  }

  @Test
  public void handleRequest_returnsNotFound() {
    Code code = Code.NOT_FOUND;
    String errorReason = "fake reason";
    String errorMessage = "fake message";

    APIGatewayProxyRequestEvent req = new APIGatewayProxyRequestEvent();
    req.setPath("/v1/encryptionKeys/abcd");
    keyDb.setServiceException(new ServiceException(code, errorReason, errorMessage));

    APIGatewayProxyResponseEvent response = lambda.handleRequest(req, null);

    assertThat(response.getStatusCode()).isEqualTo(404);
    assertThat(response.getBody())
        .isEqualTo(
            "{\n"
                + "  \"code\": 5,\n"
                + "  \"message\": \"fake message\",\n"
                + "  \"details\": [{\n"
                + "    \"reason\": \"fake reason\",\n"
                + "    \"domain\": \"\",\n"
                + "    \"metadata\": {\n"
                + "    }\n"
                + "  }]\n"
                + "}");
  }

  @Test
  public void encryptionKey_returnsActivationTimeFieldWithFalseForDisableFlag()
      throws IOException, ServiceException {
    String keyId = "abcd";
    putPrivateKey(keyId);
    APIGatewayProxyRequestEvent req = new APIGatewayProxyRequestEvent();
    req.setPath("/v1/encryptionKeys/" + keyId);

    APIGatewayProxyResponseEvent response = lambda.handleRequest(req, null);

    assertThat(response.getStatusCode()).isEqualTo(200);

    EncryptionKey.Builder builder = EncryptionKey.newBuilder();
    JsonFormat.parser().merge(response.getBody(), builder);
    EncryptionKey encryptionKey = builder.build();
    assertThat(encryptionKey.getActivationTime()).isEqualTo(12345L);
  }

  @Test
  public void encryptionKey_returnsZeroActivationTimeFieldWithTrueForDisableFlag()
      throws IOException, ServiceException {
    String keyId = "abcd";
    putPrivateKey(keyId);
    APIGatewayProxyRequestEvent req = new APIGatewayProxyRequestEvent();
    req.setPath("/v1/encryptionKeys/" + keyId);

    GetEncryptionKeyApiGatewayHandler lambda =
        new GetEncryptionKeyApiGatewayHandler(keyService, true);
    APIGatewayProxyResponseEvent response = lambda.handleRequest(req, null);

    assertThat(response.getStatusCode()).isEqualTo(200);

    EncryptionKey.Builder builder = EncryptionKey.newBuilder();
    JsonFormat.parser().merge(response.getBody(), builder);
    EncryptionKey encryptionKey = builder.build();
    assertThat(encryptionKey.getActivationTime()).isEqualTo(0);
  }

  private void putPrivateKey(String keyId) throws ServiceException {
    keyDb.createKey(
        com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto
            .EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setKeyType(EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT.name())
            .setJsonEncodedKeyset("{test}")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .setPublicKey("12345")
            .setPublicKeyMaterial("qwert")
            .setKeyEncryptionKeyUri("abc/123")
            .setActivationTime(12345L)
            .addAllKeySplitData(
                ImmutableList.of(
                    KeySplitData.newBuilder().setKeySplitKeyEncryptionKeyUri("abc/123").build()))
            .build());
  }
}
