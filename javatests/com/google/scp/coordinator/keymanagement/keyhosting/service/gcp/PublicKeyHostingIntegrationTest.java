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
import static java.time.Instant.now;
import static java.time.temporal.ChronoUnit.DAYS;
import static java.util.UUID.randomUUID;

import com.google.acai.Acai;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.KeySet;
import com.google.cloud.spanner.Mutation;
import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.PublicKeyCloudFunctionContainer;
import com.google.scp.coordinator.keymanagement.testutils.gcp.GcpKeyManagementIntegrationTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
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

/** Integration tests for GCP public key hosting cloud function */
@RunWith(JUnit4.class)
public final class PublicKeyHostingIntegrationTest {
  @Rule public Acai acai = new Acai(GcpKeyManagementIntegrationTestEnv.class);

  private static final HttpClient client = HttpClient.newHttpClient();
  private static final String correctPath = "/v1alpha/publicKeys";
  private static final String incorrectPath = "/v1alpha/wrongPath";
  @Inject @KeyDbClient private DatabaseClient dbClient;
  @Inject private SpannerKeyDb keyDb;
  @Inject @PublicKeyCloudFunctionContainer private CloudFunctionEmulatorContainer functionContainer;

  @After
  public void deleteTable() {
    dbClient.write(ImmutableList.of(Mutation.delete(SPANNER_KEY_TABLE_NAME, KeySet.all())));
  }

  @Test(timeout = 25_000)
  public void getPublicKeys_success() throws IOException, ServiceException {
    EncryptionKey encryptionKey =
        EncryptionKey.newBuilder()
            .setKeyId(randomUUID().toString())
            .setPublicKey(randomUUID().toString())
            .setPublicKeyMaterial(randomUUID().toString())
            .setJsonEncodedKeyset(randomUUID().toString())
            .setKeyEncryptionKeyUri(randomUUID().toString())
            .setCreationTime(now().toEpochMilli())
            .setExpirationTime(now().plus(2, DAYS).toEpochMilli())
            .build();
    keyDb.createKey(encryptionKey);
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(correctPath)).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);

    assertThat(httpResponse.statusCode()).isEqualTo(OK.getHttpStatusCode());
    assertThat(httpResponse.headers().allValues("cache-control").size()).isEqualTo(1);
    assertThat(httpResponse.headers().firstValue("cache-control").get()).contains("max-age=");

    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(httpResponse.body(), builder);
    GetActivePublicKeysResponse response = builder.build();
    assertThat(response.getKeysList()).hasSize(1);
    assertThat(response.getKeys(0).getId()).isEqualTo(encryptionKey.getKeyId());
    assertThat(response.getKeys(0).getKey()).isEqualTo(encryptionKey.getPublicKeyMaterial());
  }

  @Test(timeout = 25_000)
  public void getPublicKeys_emptyList() throws IOException {
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(correctPath)).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);

    assertThat(httpResponse.statusCode()).isEqualTo(OK.getHttpStatusCode());
    assertThat(httpResponse.headers().map().containsKey("cache-control")).isFalse();
    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(httpResponse.body(), builder);
    GetActivePublicKeysResponse response = builder.build();
    assertThat(response.getKeysList()).isEmpty();
  }

  @Test(timeout = 25_000)
  public void getPublicKeys_wrongPath() throws IOException {
    HttpRequest getRequest =
        HttpRequest.newBuilder().uri(getFunctionUri(incorrectPath)).GET().build();

    HttpResponse<String> httpResponse = executeRequestWithRetry(client, getRequest);
    assertThat(httpResponse.statusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());
    assertThat(httpResponse.headers().map().containsKey("cache-control")).isFalse();

    ErrorResponse.Builder builder = ErrorResponse.newBuilder();
    JsonFormat.parser().merge(httpResponse.body(), builder);
    ErrorResponse response = builder.build();
    assertThat(response.getCode()).isEqualTo(NOT_FOUND.getRpcStatusCode());
    assertThat(response.getMessage()).contains("Unsupported URL path");
    assertThat(response.getDetailsList().toString()).contains("INVALID_URL_PATH_OR_VARIABLE");
  }

  @Test(timeout = 25_000)
  public void getPublicKeys_wrongMethod() throws IOException {
    HttpRequest getRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(correctPath))
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
