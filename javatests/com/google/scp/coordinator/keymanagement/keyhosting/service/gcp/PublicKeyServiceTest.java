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
import static com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil.SPANNER_KEY_TABLE_NAME;
import static com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer.startContainerAndConnectToSpannerWithEnvs;

import com.google.acai.Acai;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.KeySet;
import com.google.cloud.spanner.Mutation;
import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
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
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey.KeyOneofCase;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
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
public final class PublicKeyServiceTest {

  private static final Logger logger = LoggerFactory.getLogger(PublicKeyServiceTest.class);

  @Rule public final Acai acai = new Acai(TestModule.class);

  @Inject private DatabaseClient dbClient;
  @Inject private KeyDb keyDb;
  @Inject private CloudFunctionEmulatorContainer container;

  @Before
  public void setUp() throws Exception {
    keyDb.createKey(FakeEncryptionKey.create());
    keyDb.createKey(FakeEncryptionKey.create().toBuilder().setSetName("test-set").build());
  }

  @Test
  public void defaultEndpoint_returnsExpectedFormat() throws Exception {
    // Given
    String endpoint = "/v1alpha/publicKeys";

    // When
    GetActivePublicKeysResponse keys = getActivePublicKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.KEY);
  }

  @Test
  public void defaultEndpoint_returnsExpectedCacheControlHeader() throws Exception {
    // Given
    String endpoint = "/v1alpha/publicKeys";

    // When
    HttpResponse response;
    String body;
    try (CloseableHttpClient client = HttpClients.createDefault()) {
      response =
          client.execute(
              new HttpGet(String.format("http://%s%s", container.getEmulatorEndpoint(), endpoint)));
    }

    // Then
    assertThat(response.getHeaders("Cache-Control")).hasLength(1);
  }

  @Test
  public void noKeys_returnsNoCacheControlHeader() throws Exception {
    // Given
    String endpoint = "/v1alpha/publicKeys";
    truncateKeyDatabase();

    // When
    HttpResponse response;
    String body;
    try (CloseableHttpClient client = HttpClients.createDefault()) {
      response =
          client.execute(
              new HttpGet(String.format("http://%s%s", container.getEmulatorEndpoint(), endpoint)));
      body = EntityUtils.toString(response.getEntity());
    }

    // Then
    GetActivePublicKeysResponse.Builder keysBuilder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(body, keysBuilder);
    GetActivePublicKeysResponse keys = keysBuilder.build();

    assertThat(keys.getKeysList()).isEmpty();
    assertThat(response.getHeaders("Cache-Control")).isEmpty();
  }

  @Test
  public void tinkEndpoint_returnsExpectedFormat() throws Exception {
    // Given
    String endpoint = "/v1alpha/publicKeys:tink";

    // When
    GetActivePublicKeysResponse keys = getActivePublicKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.TINK_BINARY);
  }

  @Test
  public void rawEndpoint_returnsExpectedFormat() throws Exception {
    // Given
    String endpoint = "/v1alpha/publicKeys:raw";

    // When
    GetActivePublicKeysResponse keys = getActivePublicKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.HPKE_PUBLIC_KEY);
  }

  @Test
  public void v1BetaDefaultEndpoint_returnsExpectedFormat() throws Exception {
    // Given
    String endpoint = "/v1beta/sets//publicKeys";

    // When
    GetActivePublicKeysResponse keys = getActivePublicKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.TINK_BINARY);
  }

  @Test
  public void v1BetaSpecificSet_returnsExpectedFormat() throws Exception {
    // Given
    String endpoint = "/v1beta/sets/test-set/publicKeys";

    // When
    GetActivePublicKeysResponse keys = getActivePublicKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.TINK_BINARY);
  }

  @Test
  public void v1BetaRawEndpoint_returnsExpectedFormat() throws Exception {
    // Given
    String endpoint = "/v1beta/sets//publicKeys:raw";

    // When
    GetActivePublicKeysResponse keys = getActivePublicKeys(endpoint);

    // Then
    assertThat(keys.getKeysList()).isNotEmpty();
    assertThat(keys.getKeys(0).getKeyOneofCase()).isEqualTo(KeyOneofCase.HPKE_PUBLIC_KEY);
  }

  private GetActivePublicKeysResponse getActivePublicKeys(String endpoint)
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
      GetActivePublicKeysResponse.Builder keysBuilder = GetActivePublicKeysResponse.newBuilder();
      JsonFormat.parser().merge(body, keysBuilder);
      return keysBuilder.build();
    }
  }

  private void truncateKeyDatabase() {
    dbClient.write(ImmutableList.of(Mutation.delete(SPANNER_KEY_TABLE_NAME, KeySet.all())));
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
              "PublicKeyService_deploy.jar",
              "java/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp/",
              "com.google.scp.coordinator.keymanagement.keyhosting.service.gcp.PublicKeyService");
      container.followOutput(new Slf4jLogConsumer(logger).withSeparateOutputStreams());
      return container;
    }
  }
}
