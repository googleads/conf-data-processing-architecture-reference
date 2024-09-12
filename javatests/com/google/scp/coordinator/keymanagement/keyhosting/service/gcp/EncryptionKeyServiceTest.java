/*
 * Copyright 2024 Google LLC
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
package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer.startContainerAndConnectToSpannerWithEnvs;

import com.google.acai.Acai;
import com.google.cloud.spanner.DatabaseClient;
import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbConfig;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbTestModule;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainer;
import java.io.IOException;
import java.util.Optional;
import org.apache.http.HttpResponse;
import org.apache.http.client.config.RequestConfig;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClientBuilder;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.testcontainers.containers.output.Slf4jLogConsumer;

@RunWith(JUnit4.class)
public final class EncryptionKeyServiceTest {

  private static final Logger logger = LoggerFactory.getLogger(EncryptionKeyServiceTest.class);
  private static final EncryptionKey DEFAULT_SET_KEY = FakeEncryptionKey.create();
  private static final EncryptionKey TEST_SET_KEY =
      FakeEncryptionKey.create().toBuilder().setSetName("test-set").build();

  @Rule public final Acai acai = new Acai(TestModule.class);

  @Inject private DatabaseClient dbClient;
  @Inject private KeyDb keyDb;
  @Inject private CloudFunctionEmulatorContainer container;

  @Before
  public void setUp() throws Exception {
    keyDb.createKey(DEFAULT_SET_KEY);
    keyDb.createKey(TEST_SET_KEY);
  }

  @Test
  public void v1alphaGetEncryptionKey_existingKey_returnsExpected() throws Exception {
    // Given
    String endpoint = String.format("/v1alpha/encryptionKeys/%s", DEFAULT_SET_KEY.getKeyId());

    // When
    EncryptionKeyProto.EncryptionKey key = getEncryptionKey(endpoint);

    // Then
    assertThat(key.getName()).endsWith(DEFAULT_SET_KEY.getKeyId());
  }

  @Test
  public void v1alphaGetEncryptionKey_nonExistingKey_returnsNotFound() throws Exception {
    // Given
    String endpoint = String.format("/v1alpha/encryptionKeys/%s", "non-existing-key");

    // When
    HttpResponse response = getHttpResponse(endpoint);

    // Then
    assertThat(response.getStatusLine().getStatusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());
  }

  @Test
  public void v1alphaListRecentEncryptionKeys_happyPath_returnsExpected() throws Exception {
    // Given
    String endpoint = "/v1alpha/encryptionKeys:recent?maxAgeSeconds=999";

    // When
    ListRecentEncryptionKeysResponse keys = listRecentEncryptionKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
  }

  @Test
  public void v1alphaListRecentEncryptionKeys_missingMaxAgeSeconds_returnsExpectedError()
      throws Exception {
    // Given
    String endpoint = "/v1alpha/encryptionKeys:recent";

    // When
    HttpResponse response = getHttpResponse(endpoint);

    // Then
    assertThat(response.getStatusLine().getStatusCode())
        .isEqualTo(INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void v1betaGetEncryptionKey_existingKey_returnsExpected() throws Exception {
    // Given
    String endpoint = String.format("/v1beta/encryptionKeys/%s", DEFAULT_SET_KEY.getKeyId());

    // When
    EncryptionKeyProto.EncryptionKey key = getEncryptionKey(endpoint);

    // Then
    assertThat(key.getName()).endsWith(DEFAULT_SET_KEY.getKeyId());
  }

  @Test
  public void v1betaGetEncryptionKey_nonExistingKey_returnsNotFound() throws Exception {
    // Given
    String endpoint = String.format("/v1beta/encryptionKeys/%s", "non-existing-key");

    // When
    HttpResponse response = getHttpResponse(endpoint);

    // Then
    assertThat(response.getStatusLine().getStatusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());
  }

  @Test
  public void v1betaListRecentEncryptionKeys_happyPath_returnsExpected() throws Exception {
    // Given
    String endpoint = "/v1beta/sets//encryptionKeys:recent?maxAgeSeconds=999";

    // When
    ListRecentEncryptionKeysResponse keys = listRecentEncryptionKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
  }

  @Test
  public void v1betaListRecentEncryptionKeys_specificSetName_returnsExpected() throws Exception {
    // Given
    String endpoint = "/v1beta/sets/test-set/encryptionKeys:recent?maxAgeSeconds=999";

    // When
    ListRecentEncryptionKeysResponse keys = listRecentEncryptionKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
  }

  @Test
  public void v1betaListRecentEncryptionKeys_missingMaxAgeSeconds_returnsExpectedError()
      throws Exception {
    // Given
    String endpoint = "/v1beta/sets//encryptionKeys:recent";

    // When
    HttpResponse response = getHttpResponse(endpoint);

    // Then
    assertThat(response.getStatusLine().getStatusCode())
        .isEqualTo(INVALID_ARGUMENT.getHttpStatusCode());
  }

  private EncryptionKeyProto.EncryptionKey getEncryptionKey(String endpoint) throws IOException {
    HttpClientBuilder builder =
        HttpClients.custom()
            .setDefaultRequestConfig(
                // Prevent HTTP client library from stripping empty path params.
                RequestConfig.custom().setNormalizeUri(false).build());
    try (CloseableHttpClient client = builder.build()) {
      HttpResponse response =
          client.execute(
              new HttpGet(String.format("http://%s%s", container.getEmulatorEndpoint(), endpoint)));
      String body = EntityUtils.toString(response.getEntity());
      EncryptionKeyProto.EncryptionKey.Builder keysBuilder =
          EncryptionKeyProto.EncryptionKey.newBuilder();
      JsonFormat.parser().merge(body, keysBuilder);
      return keysBuilder.build();
    }
  }

  private ListRecentEncryptionKeysResponse listRecentEncryptionKeys(String endpoint)
      throws IOException {
    HttpClientBuilder builder =
        HttpClients.custom()
            .setDefaultRequestConfig(
                // Prevent HTTP client library from stripping empty path params.
                RequestConfig.custom().setNormalizeUri(false).build());
    try (CloseableHttpClient client = builder.build()) {
      HttpResponse response =
          client.execute(
              new HttpGet(String.format("http://%s%s", container.getEmulatorEndpoint(), endpoint)));
      String body = EntityUtils.toString(response.getEntity());
      ListRecentEncryptionKeysResponse.Builder keysBuilder =
          ListRecentEncryptionKeysResponse.newBuilder();
      JsonFormat.parser().merge(body, keysBuilder);
      return keysBuilder.build();
    }
  }

  private HttpResponse getHttpResponse(String endpoint) throws IOException {
    HttpClientBuilder builder =
        HttpClients.custom()
            .setDefaultRequestConfig(
                // Prevent HTTP client library from stripping empty path params.
                RequestConfig.custom().setNormalizeUri(false).build());
    try (CloseableHttpClient client = builder.build()) {
      HttpResponse response =
          client.execute(
              new HttpGet(String.format("http://%s%s", container.getEmulatorEndpoint(), endpoint)));
      return response;
    }
  }

  private static final class TestModule extends AbstractModule {
    @Override
    protected void configure() {
      install(new SpannerKeyDbTestModule());
    }

    @Singleton
    @Provides
    CloudFunctionEmulatorContainer start(
        KeyDb keyDb,
        SpannerKeyDbConfig keyDbConfig,
        SpannerEmulatorContainer spannerEmulatorContainer) {
      Preconditions.checkNotNull(keyDb, "Key database is required to start key database.");
      var container =
          startContainerAndConnectToSpannerWithEnvs(
              spannerEmulatorContainer,
              Optional.of(
                  ImmutableMap.of(
                      "SPANNER_INSTANCE", keyDbConfig.spannerInstanceId(),
                      "SPANNER_DATABASE", keyDbConfig.spannerDbName(),
                      "PROJECT_ID", keyDbConfig.gcpProjectId())),
              "EncryptionKeyService_deploy.jar",
              "java/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp/",
              "com.google.scp.coordinator.keymanagement.keyhosting.service.gcp.EncryptionKeyService");
      container.followOutput(new Slf4jLogConsumer(logger).withSeparateOutputStreams());
      return container;
    }
  }
}
