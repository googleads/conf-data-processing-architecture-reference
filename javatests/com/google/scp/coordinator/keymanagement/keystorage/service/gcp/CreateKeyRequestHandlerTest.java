/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.OK;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.acai.Acai;
import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.crypto.tink.Aead;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.service.common.KeyStorageService;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.GetDataKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.gcp.GcpCreateKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringReader;
import java.io.StringWriter;
import java.security.GeneralSecurityException;
import java.util.Base64;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class CreateKeyRequestHandlerTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Inject private InMemoryKeyDb keyDb;

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;
  @Mock Aead mockAead;
  // Unused in GCP implementation.
  @Mock GetDataKeyTask getDataKeyTask;

  private BufferedWriter writer;
  private CreateKeyRequestHandler requestHandler;

  @Before
  public void before() throws IOException {
    KeyStorageService keyStorageService =
        new KeyStorageService(new GcpCreateKeyTask(keyDb, mockAead, "URI"), getDataKeyTask);
    requestHandler = new CreateKeyRequestHandler(keyStorageService);
    writer = new BufferedWriter(new StringWriter());
    when(httpResponse.getWriter()).thenReturn(writer);
    when(httpRequest.getMethod()).thenReturn("POST");
  }

  @After
  public void after() {
    keyDb.reset();
  }

  @Test
  public void handleRequest_validJson() throws IOException, GeneralSecurityException {
    String keyId = "12345";
    String name = "keys/" + keyId;
    String encryptionKeyType = "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT";
    String publicKeysetHandle = "myPublicKeysetHandle";
    String publicKeyMaterial = Base64.getEncoder().encodeToString("myPublicKeyMaterial".getBytes());
    String privateKeySplit = Base64.getEncoder().encodeToString("myPrivateKeySplit".getBytes());
    String creationTime = "0";
    String expirationTime = "0";
    String testKeyData =
        "{\"publicKeySignature\": \"a\", \"keyEncryptionKeyUri\": \"b\", \"keyMaterial\": \"c\"}";
    String keyData = "[" + testKeyData + "," + testKeyData + "]";
    String jsonRequest =
        "{\"keyId\":\""
            + keyId
            + "\",\"encryptedKeySplit\":\""
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
    setRequestBody(jsonRequest);
    when(mockAead.decrypt("myPrivateKeySplit".getBytes(), "myPublicKeyMaterial".getBytes()))
        .thenReturn("decryptSuccessful".getBytes());
    when(mockAead.encrypt("decryptSuccessful".getBytes(), new byte[0]))
        .thenReturn("validatedKeySplit".getBytes());

    requestHandler.handleRequest(httpRequest, httpResponse);

    writer.flush();
    verify(httpResponse).setStatusCode(eq(OK.getHttpStatusCode()));
    assertThat(getPrivateKey(keyId).getJsonEncodedKeyset())
        .isEqualTo(Base64.getEncoder().encodeToString("validatedKeySplit".getBytes()));
  }

  @Test
  public void handleRequest_invalidJson() throws IOException {
    String jsonRequest = "asdf}{";
    setRequestBody(jsonRequest);

    requestHandler.handleRequest(httpRequest, httpResponse);

    writer.flush();
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
  }

  @Test
  public void handleRequest_invalidHttpMethod() throws IOException, GeneralSecurityException {
    String keyId = "12345";
    String name = "keys/" + keyId;
    String publicKey = "myPublicKey";
    String privateKeySplit = Base64.getEncoder().encodeToString("myPrivateKeySplit".getBytes());
    String expirationTime = "0";
    String jsonRequest =
        "{\"keyId\":\""
            + keyId
            + "\",\"encryptedKeySplit\":\""
            + privateKeySplit
            + "\",\"key\":{"
            + "\"name\":\""
            + name
            + "\",\"publicKey\":\""
            + publicKey
            + "\",\"expirationTime\":"
            + expirationTime
            + "}}";
    setRequestBody(jsonRequest);
    lenient().when(mockAead.decrypt(any(), any())).thenReturn("decryptSuccessful".getBytes());
    when(httpRequest.getMethod()).thenReturn("PUT");

    requestHandler.handleRequest(httpRequest, httpResponse);

    writer.flush();
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
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

  private void setRequestBody(String body) throws IOException {
    BufferedReader reader = new BufferedReader(new StringReader(body));
    lenient().when(httpRequest.getReader()).thenReturn(reader);
  }
}
