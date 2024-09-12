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

package com.google.scp.coordinator.keymanagement.keystorage.service.aws;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.OK;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.acai.Acai;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.PublicKeySign;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.service.common.KeyStorageService;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.aws.AwsCreateKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.GetDataKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.SignDataKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeDataKeyUtil;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import java.util.Base64;
import java.util.Optional;
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
public class CreateKeyApiGatewayHandlerTest {

  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Inject private InMemoryKeyDb keyDb;
  @Mock Aead mockAead;
  @Mock private PublicKeySign publicKeySign;
  private final CloudAeadSelector aeadSelector = FakeDataKeyUtil.getAeadSelector();
  @Mock GetDataKeyTask getDataKeyTask;
  @Mock SignDataKeyTask signDataKeyTask;

  private CreateKeyApiGatewayHandler lambda;

  @Before
  public void getInstances() {
    KeyStorageService keyStorageService =
        new KeyStorageService(
            new AwsCreateKeyTask(
                keyDb, mockAead, "b", Optional.of(publicKeySign), aeadSelector, signDataKeyTask),
            getDataKeyTask);
    lambda = new CreateKeyApiGatewayHandler(keyStorageService);
  }

  @Test
  public void handleRequest_validJson() throws Exception {
    var dataKey = FakeDataKeyUtil.createDataKey();
    String keyId = "12345";
    String name = "keys/" + keyId;
    String encryptionKeyType = "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT";
    String publicKeysetHandle = "myPublicKeysetHandle";
    String publicKeyMaterial = Base64.getEncoder().encodeToString("myPublicKeyMaterial".getBytes());
    String privateKeySplit =
        FakeDataKeyUtil.encryptString(dataKey, "fake-split", publicKeyMaterial);
    String creationTime = "0";
    String expirationTime = "0";
    String testKeyData =
        "{\"publicKeySignature\": \"a\", \"keyEncryptionKeyUri\": \"b\", \"keyMaterial\": \"c\"}";
    String keyData = "[" + testKeyData + "," + testKeyData + "]";
    String jsonRequest =
        "{\"keyId\":\""
            + keyId
            + "\",\"keySplitEncryptionType\":\""
            + "DATA_KEY"
            + "\",\"encryptedKeySplitDataKey\":{"
            + "\"encryptedDataKey\":\""
            + dataKey.getEncryptedDataKey()
            + "\",\"encryptedDataKeyKekUri\":\""
            + dataKey.getEncryptedDataKeyKekUri()
            + "\"},\"encryptedKeySplit\":\""
            + privateKeySplit
            + "\",\"key\":{"
            + "\"name\":\""
            + name
            + "\",\"encryptionKeyType\":\""
            + encryptionKeyType
            + "\",\"publicKeysetHandle\":\""
            + publicKeysetHandle
            + "\",\"publicKeyMaterial\":\""
            + publicKeyMaterial
            + "\",\"creationTime\":\""
            + creationTime
            + "\",\"expirationTime\":\""
            + expirationTime
            + "\",\"keyData\":"
            + keyData
            + "}}";
    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setBody(jsonRequest);
    when(mockAead.encrypt(any(), any())).thenReturn("key_encrypted".getBytes());
    when(publicKeySign.sign(any(byte[].class))).thenReturn("signature".getBytes());

    APIGatewayProxyResponseEvent response = lambda.handleRequest(request, null);

    var encryptRequestMatcher = ArgumentCaptor.forClass(byte[].class);
    verify(mockAead, times(1)).encrypt(encryptRequestMatcher.capture(), any());
    assertThat(encryptRequestMatcher.getValue()).isEqualTo("fake-split".getBytes());
    assertThat(response.getStatusCode()).isEqualTo(OK.getHttpStatusCode());
    assertThat(getPrivateKey(keyId).getJsonEncodedKeyset())
        .isEqualTo(Base64.getEncoder().encodeToString("key_encrypted".getBytes()));
  }

  @Test
  public void handleRequest_invalidJson() {
    String jsonRequest = "asdf}{";
    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setBody(jsonRequest);

    APIGatewayProxyResponseEvent response = lambda.handleRequest(request, null);

    assertThat(response.getStatusCode()).isEqualTo(INVALID_ARGUMENT.getHttpStatusCode());
    assertThat(response.getBody()).contains("Failed to parse CreateKeyRequest");
  }

  @Test
  public void handleRequest_emptyBody() {
    // Leave body as default value of null.
    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();

    APIGatewayProxyResponseEvent response = lambda.handleRequest(request, null);

    assertThat(response.getStatusCode()).isEqualTo(INVALID_ARGUMENT.getHttpStatusCode());
    assertThat(response.getBody()).contains("Missing request body");
  }

  private EncryptionKey getPrivateKey(String keyId) {
    EncryptionKey result;

    try {
      result = keyDb.getKey(keyId);
    } catch (ServiceException ex) {
      result = null;
    }

    return result;
  }
}
