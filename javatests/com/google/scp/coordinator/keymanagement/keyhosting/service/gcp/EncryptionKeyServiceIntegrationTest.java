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

package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil.SPANNER_KEY_TABLE_NAME;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.testutils.common.HttpRequestUtil.executeRequestWithRetry;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.google.acai.Acai;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.KeySet;
import com.google.cloud.spanner.Mutation;
import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.EncryptionKeyServiceCloudFunctionContainer;
import com.google.scp.coordinator.keymanagement.testutils.gcp.GcpKeyManagementIntegrationTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.util.ErrorUtil;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpRequest.BodyPublishers;
import java.net.http.HttpResponse;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Integration tests for GCP encryption key service cloud function */
@RunWith(JUnit4.class)
public final class EncryptionKeyServiceIntegrationTest {
  @Rule public Acai acai = new Acai(GcpKeyManagementIntegrationTestEnv.class);

  private static final HttpClient client = HttpClient.newHttpClient();
  private static final String correctPath = "/v1alpha/encryptionKeys/";
  private static final String incorrectPath = "/v1alpha/wrongPath/";

  private static final String correctListRecentKeysPath =
      "/v1alpha/encryptionKeys:recent?maxAgeSeconds=999";
  private static final String uncompletedListRecentKeysPath = "/v1alpha/encryptionKeys:recent";
  private static final String incorrectListRecentKeysPath =
      "/v1alpha/encryptionKeys:wrong?maxAgeSeconds=999";
  @Inject @KeyDbClient private DatabaseClient dbClient;
  @Inject private SpannerKeyDb keyDb;

  @Inject @EncryptionKeyServiceCloudFunctionContainer
  private CloudFunctionEmulatorContainer functionContainer;

  @After
  public void deleteTable() {
    dbClient.write(ImmutableList.of(Mutation.delete(SPANNER_KEY_TABLE_NAME, KeySet.all())));
  }

  @Test(timeout = 25_000)
  public void getEncryptionKeys_success() throws ServiceException {
    EncryptionKey encryptionKey = FakeEncryptionKey.create();

    keyDb.createKey(encryptionKey);
    HttpRequest getRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(correctPath + encryptionKey.getKeyId()))
            .GET()
            .build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);

    assertThat(httpResponse.statusCode()).isEqualTo(OK.getHttpStatusCode());
    // Cannot map to abstract autovalue class, so the body String will be parsed instead
    String response = httpResponse.body();
    assertThat(response).contains("\"name\": \"encryptionKeys/");
    assertThat(response).contains("\"keyMaterial\": \"" + encryptionKey.getJsonEncodedKeyset());
  }

  @Test(timeout = 25_000)
  public void getEncryptionKeys_notFound() throws JsonProcessingException {
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(correctPath + "invalid")).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);

    assertThat(httpResponse.statusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());

    ErrorResponse response = ErrorUtil.parseErrorResponse(httpResponse.body());
    assertThat(response.getCode()).isEqualTo(NOT_FOUND.getRpcStatusCode());
    assertThat(response.getMessage()).contains("Unable to find item with keyId");
    assertThat(response.getDetailsList().toString()).contains("MISSING_KEY");
  }

  @Test(timeout = 25_000)
  public void getEncryptionKeys_wrongPath() throws JsonProcessingException {
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(incorrectPath)).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);

    assertThat(httpResponse.statusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());

    ErrorResponse response = ErrorUtil.parseErrorResponse(httpResponse.body());
    assertThat(response.getCode()).isEqualTo(NOT_FOUND.getRpcStatusCode());
    assertThat(response.getMessage()).contains("Unsupported URL path");
    assertThat(response.getDetailsList().toString()).contains("INVALID_URL_PATH_OR_VARIABLE");
  }

  @Test(timeout = 25_000)
  public void getEncryptionKeys_wrongMethod() throws JsonProcessingException {
    HttpRequest getRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(correctPath))
            .POST(BodyPublishers.noBody())
            .build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);

    assertThat(httpResponse.statusCode()).isEqualTo(INVALID_ARGUMENT.getHttpStatusCode());
    assertThat(httpResponse.headers().map().containsKey("cache-control")).isFalse();

    ErrorResponse response = ErrorUtil.parseErrorResponse(httpResponse.body());
    assertThat(response.getCode()).isEqualTo(INVALID_ARGUMENT.getRpcStatusCode());
    assertThat(response.getMessage()).contains("Unsupported http method");
    assertThat(response.getDetailsList().toString()).contains("INVALID_HTTP_METHOD");
  }

  @Test(timeout = 25_000)
  public void getRecentKeys_success() throws IOException, ServiceException {
    keyDb.createKey(FakeEncryptionKey.create());
    keyDb.createKey(FakeEncryptionKey.create());
    System.out.println("\n  test out: ");
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(correctListRecentKeysPath)).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);

    assertThat(httpResponse.statusCode()).isEqualTo(OK.getHttpStatusCode());

    ListRecentEncryptionKeysResponse.Builder builder =
        ListRecentEncryptionKeysResponse.newBuilder();
    JsonFormat.parser().merge(httpResponse.body(), builder);
    ListRecentEncryptionKeysResponse response = builder.build();
    assertThat(response.getKeysList()).hasSize(2);
  }

  @Test(timeout = 25_000)
  public void listRecentKeys_emptyList() throws IOException {
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(correctListRecentKeysPath)).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);

    assertThat(httpResponse.statusCode()).isEqualTo(OK.getHttpStatusCode());

    ListRecentEncryptionKeysResponse.Builder builder =
        ListRecentEncryptionKeysResponse.newBuilder();
    JsonFormat.parser().merge(httpResponse.body(), builder);
    ListRecentEncryptionKeysResponse response = builder.build();
    assertThat(response.getKeysList()).hasSize(0);
  }

  @Test(timeout = 25_000)
  public void listRecentKeys_missingMaxAgeSeconds() throws IOException {
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(uncompletedListRecentKeysPath)).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);
    assertThat(httpResponse.statusCode()).isEqualTo(INVALID_ARGUMENT.getHttpStatusCode());

    ErrorResponse.Builder builder = ErrorResponse.newBuilder();
    JsonFormat.parser().merge(httpResponse.body(), builder);
    ErrorResponse response = builder.build();
    assertThat(response.getCode()).isEqualTo(INVALID_ARGUMENT.getRpcStatusCode());
    assertThat(response.getMessage()).contains("maxAgeSeconds query parameter is required.");
  }

  @Test(timeout = 25_000)
  public void listRecentKeys_wrongPath() throws IOException {
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(incorrectListRecentKeysPath)).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);
    assertThat(httpResponse.statusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());

    ErrorResponse.Builder builder = ErrorResponse.newBuilder();
    JsonFormat.parser().merge(httpResponse.body(), builder);
    ErrorResponse response = builder.build();
    assertThat(response.getCode()).isEqualTo(NOT_FOUND.getRpcStatusCode());
    assertThat(response.getMessage()).contains("Unsupported URL path");
    assertThat(response.getDetailsList().toString()).contains("INVALID_URL_PATH_OR_VARIABLE");
  }

  @Test(timeout = 25_000)
  public void listRecentKeys_wrongMethod() throws IOException {
    HttpRequest getRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(correctListRecentKeysPath))
            .POST(BodyPublishers.noBody())
            .build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);
    assertThat(httpResponse.statusCode()).isEqualTo(INVALID_ARGUMENT.getHttpStatusCode());
    assertThat(httpResponse.headers().map().containsKey("cache-control")).isFalse();

    ErrorResponse.Builder builder = ErrorResponse.newBuilder();
    JsonFormat.parser().merge(httpResponse.body(), builder);
    ErrorResponse response = builder.build();
    assertThat(response.getCode()).isEqualTo(INVALID_ARGUMENT.getRpcStatusCode());
    assertThat(response.getMessage()).contains("Unsupported http method");
    assertThat(response.getDetailsList().toString()).contains("INVALID_HTTP_METHOD");
  }

  private URI getFunctionUri(String path) {
    return URI.create("http://" + functionContainer.getEmulatorEndpoint() + path);
  }
}
