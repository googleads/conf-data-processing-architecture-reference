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

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.AwsKeyManagementIntegrationTestEnv;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAEncryptionKeyServiceBaseUrl;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import javax.inject.Inject;
import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;
import org.junit.Rule;
import org.junit.Test;

/**
 * Integration test which deploys DDB, API Gateway, and the EncryptionKeyHosting lambda to
 * LocalStack. Tests the lambda via HTTP.
 */
public final class GetEncryptionKeyApiGatewayHandlerIntegrationTest {
  @Rule public Acai acai = new Acai(AwsKeyManagementIntegrationTestEnv.class);
  @Inject private DynamoKeyDb keyDb;
  @Inject @CoordinatorAEncryptionKeyServiceBaseUrl private String baseUrl;
  private final HttpClient httpClient = HttpClients.createDefault();

  @Test
  public void returnsPrivateKey() throws Exception {
    var keyId = "abcd";
    var jsonEncodedKeyset = "{foo}";
    var encryptionKey =
        com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto
            .EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setKeyType(EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT.name())
            .setPublicKey("{}")
            .setJsonEncodedKeyset(jsonEncodedKeyset)
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .setKeyEncryptionKeyUri("abc/123")
            .setPublicKeyMaterial("qwert")
            .addAllKeySplitData(
                ImmutableList.of(
                    KeySplitData.newBuilder()
                        .setKeySplitKeyEncryptionKeyUri("abc/123")
                        .setPublicKeySignature("456")
                        .build()))
            .build();
    keyDb.createKey(encryptionKey);

    var response = fetchKey(keyId);
    System.out.print(response);

    EncryptionKey.Builder builder = EncryptionKey.newBuilder();
    JsonFormat.parser().merge(getResponseBody(response), builder);
    EncryptionKey privateKeyResponse = builder.build();

    assertThat(privateKeyResponse.getName()).isEqualTo("encryptionKeys/abcd");
  }

  @Test
  public void returnsServiceExceptionNotFound() throws Exception {
    var response = fetchKey("unknown");
    ErrorResponse.Builder builder = ErrorResponse.newBuilder();
    JsonFormat.parser().merge(getResponseBody(response), builder);
    ErrorResponse errorResponse = builder.build();

    assertThat(errorResponse.getMessage()).contains("Unable to find");
    assertThat(errorResponse.getCode()).isEqualTo(5 /* RPC code for not found */);
    assertThat(response.getStatusLine().getStatusCode()).isEqualTo(404);
  }

  private HttpResponse fetchKey(String keyId) throws Exception {
    var request = new HttpGet(baseUrl + "/encryptionKeys/" + keyId);
    return httpClient.execute(request);
  }

  private String getResponseBody(HttpResponse response) throws Exception {
    return EntityUtils.toString(response.getEntity());
  }
}
