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

package com.google.scp.coordinator.keymanagement.keyhosting.service.aws;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.inject.AbstractModule;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keyhosting.service.aws.testing.LocalListRecentEncryptionKeysModule;
import com.google.scp.coordinator.keymanagement.keyhosting.service.aws.testing.LocalListRecentEncryptionKeysModule.ListRecentEncryptionKeysBaseUrl;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.KeyDbIntegrationTestModule;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestModule;
import javax.inject.Inject;
import org.apache.http.HttpResponse;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;
import org.junit.Rule;
import org.junit.Test;

/**
 * Integration test which deploys DynamoDB, API Gateway, and the ListRecentEncryptionKeys Lambda
 * function to LocalStack and available over HTTP.
 */
public final class ListRecentEncryptionKeysApiGatewayHandlerIntegrationTest {
  @Rule public Acai acai = new Acai(TestModule.class);

  static class TestModule extends AbstractModule {
    @Override
    public void configure() {
      install(new AwsIntegrationTestModule());
      install(new KeyDbIntegrationTestModule());
      install(new LocalListRecentEncryptionKeysModule());
    }
  }

  @Inject private DynamoKeyDb keyDb;
  @Inject @ListRecentEncryptionKeysBaseUrl private String baseUrl;

  @Test
  public void http_happyPath_returnsExpected() throws Exception {
    // Given
    keyDb.createKey(FakeEncryptionKey.create());
    keyDb.createKey(FakeEncryptionKey.create());

    // When
    ListRecentEncryptionKeysResponse response;
    try (CloseableHttpClient httpClient = HttpClients.createDefault()) {
      // When
      HttpGet httpGet = new HttpGet(baseUrl + "/encryptionKeys:recent?maxAgeSeconds=999");
      try (CloseableHttpResponse httpResponse = httpClient.execute(httpGet)) {
        ListRecentEncryptionKeysResponse.Builder builder =
            ListRecentEncryptionKeysResponse.newBuilder();
        JsonFormat.parser().merge(getResponseBody(httpResponse), builder);
        response = builder.build();
      }
    }

    // Then
    assertThat(response.getKeysList()).hasSize(2);
  }

  @Test
  public void http_badMaxAgeSecondsValues_returnsExpectedError() throws Exception {
    // Given
    ImmutableList<String> badValues =
        ImmutableList.of("-10", "not-a-number", "0x123", "99999999999999999");

    // When
    ImmutableList.Builder<ErrorResponse> errors = ImmutableList.builder();
    for (String badValue : badValues) {
      try (CloseableHttpClient httpClient = HttpClients.createDefault()) {
        // When
        HttpGet httpGet = new HttpGet(baseUrl + "/encryptionKeys:recent?maxAgeSeconds=" + badValue);
        try (CloseableHttpResponse httpResponse = httpClient.execute(httpGet)) {
          ErrorResponse.Builder builder = ErrorResponse.newBuilder();
          JsonFormat.parser().merge(getResponseBody(httpResponse), builder);
          errors.add(builder.build());
        }
      }
    }

    // Then
    for (ErrorResponse error : errors.build()) {
      assertThat(error.getCode()).isEqualTo(INVALID_ARGUMENT.getRpcStatusCode());
    }
  }

  private static String getResponseBody(HttpResponse response) throws Exception {
    return EntityUtils.toString(response.getEntity());
  }
}
