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
import static com.google.scp.coordinator.keymanagement.keyhosting.service.aws.testing.LocalPublicKeyHostingServiceModule.PublicKeyLambdaArn;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.expectedEncodedPublicKey;
import static java.time.Instant.now;
import static java.time.temporal.ChronoUnit.DAYS;
import static java.util.UUID.randomUUID;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.acai.Acai;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.AwsKeyManagementIntegrationTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.mapper.GuavaObjectMapper;
import com.google.scp.shared.testutils.aws.LambdaResponsePayload;
import java.util.Map;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.lambda.model.InvokeRequest;
import software.amazon.awssdk.services.lambda.model.InvokeResponse;

@RunWith(JUnit4.class)
public final class PublicKeyHostingIntegrationTest {

  @Rule public Acai acai = new Acai(AwsKeyManagementIntegrationTestEnv.class);
  private final ObjectMapper mapper = new GuavaObjectMapper();
  @Inject private LambdaClient lambdaClient;
  @Inject private DynamoKeyDb dynamoKeyDb;
  @Inject @PublicKeyLambdaArn private String publicKeyLambdaArn;

  @Test(timeout = 120_000)
  public void getPublicKeys_success()
      throws JsonProcessingException, ServiceException, InvalidProtocolBufferException {
    EncryptionKey encryptionKey =
        EncryptionKey.newBuilder()
            .setKeyId(randomUUID().toString())
            .setPublicKey(randomUUID().toString())
            .setPublicKeyMaterial(randomUUID().toString())
            .setJsonEncodedKeyset(randomUUID().toString())
            .setCreationTime(now().minus(7, DAYS).toEpochMilli())
            .setExpirationTime(now().plus(7, DAYS).toEpochMilli())
            .build();
    dynamoKeyDb.createKey(encryptionKey);

    LambdaResponsePayload keysPayload = invokeAndValidateLambdaResponse();

    // Verify key
    assertThat(keysPayload.body()).isNotNull();
    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(keysPayload.body(), builder);
    GetActivePublicKeysResponse keysResponse = builder.build();
    assertThat(keysResponse.getKeysList()).hasSize(1);
    assertThat(keysResponse.getKeys(0)).isEqualTo(expectedEncodedPublicKey(encryptionKey));

    // Verify headers
    Map<String, String> headers = keysPayload.headers();
    assertThat(headers).hasSize(2);
    assertThat(headers).containsKey("Cache-Control");
    assertThat(headers.get("Cache-Control")).startsWith("max-age=");
    assertThat(headers).containsKey("content-type");
    assertThat(headers.get("content-type")).startsWith("application/json");
  }

  @Test(timeout = 120_000)
  public void getPublicKeys_emptyList()
      throws JsonProcessingException, InvalidProtocolBufferException {
    LambdaResponsePayload keysPayload = invokeAndValidateLambdaResponse();

    // Verify keys are empty
    assertThat(keysPayload.body()).isNotNull();
    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(keysPayload.body(), builder);
    GetActivePublicKeysResponse keysResponse = builder.build();
    assertThat(keysResponse.getKeysList()).isEmpty();

    // Verify content-type header was set, at a minimum
    assertThat(keysPayload.headers().size()).isEqualTo(1);
    assertThat(keysPayload.headers()).containsKey("content-type");
    assertThat(keysPayload.headers().get("content-type")).startsWith("application/json");
  }

  private LambdaResponsePayload invokeAndValidateLambdaResponse() throws JsonProcessingException {
    InvokeResponse response =
        lambdaClient.invoke(InvokeRequest.builder().functionName(publicKeyLambdaArn).build());

    // Get response body from invocation
    String responseBody = response.payload().asUtf8String();
    LambdaResponsePayload keysPayload = mapper.readValue(responseBody, LambdaResponsePayload.class);
    assertThat(keysPayload).isNotNull();
    assertThat(keysPayload.statusCode()).isEqualTo(200);

    return keysPayload;
  }
}
