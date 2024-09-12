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

package com.google.scp.operator.shared.dao.metadatadb.gcp;

import com.google.cloud.NoCredentials;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.DatabaseId;
import com.google.cloud.spanner.InstanceId;
import com.google.cloud.spanner.LazySpannerInitializer;
import com.google.cloud.spanner.Spanner;
import com.google.cloud.spanner.SpannerOptions;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb.JobDbClient;

/** Module for Spanner job DB. */
public final class SpannerJobDbModule extends AbstractModule {

  private static final LazySpannerInitializer SPANNER_INITIALIZER = new LazySpannerInitializer();

  /**
   * Creates a new instance of the {@code SpannerJobDbModule} class.
   *
   * <p>Caller is expected to bind {@link SpannerMetadataDbConfig}.
   */
  public SpannerJobDbModule() {}

  /** Provides a new instance of the {@code DatabaClient} class. */
  @Provides
  @Singleton
  @JobDbClient
  public DatabaseClient getDatabaseClient(SpannerMetadataDbConfig config) throws Exception {
    if (config.endpointUrl().isPresent()) {
      return getDatabaseClientByEndpointUrl(config);
    }
    DatabaseId dbId =
        DatabaseId.of(config.gcpProjectId(), config.spannerInstanceId(), config.spannerDbName());
    return SPANNER_INITIALIZER.get().getDatabaseClient(dbId);
  }

  /**
   * Configures injected dependencies for this module. Includes a binding for the {@code JobDb}
   * class.
   */
  @Override
  protected void configure() {
    bind(JobDb.class).to(SpannerJobDb.class);
  }

  private static DatabaseClient getDatabaseClientByEndpointUrl(SpannerMetadataDbConfig config) {
    String endpointUrl = config.endpointUrl().get();
    SpannerOptions.Builder spannerOptions =
        SpannerOptions.newBuilder().setProjectId(config.gcpProjectId());
    if (isEmulatorEndpoint(endpointUrl)) {
      spannerOptions.setEmulatorHost(endpointUrl).setCredentials(NoCredentials.getInstance());
    } else {
      spannerOptions.setHost(endpointUrl);
    }
    Spanner spanner = spannerOptions.build().getService();
    InstanceId instanceId = InstanceId.of(config.gcpProjectId(), config.spannerInstanceId());
    DatabaseId databaseId = DatabaseId.of(instanceId, config.spannerDbName());
    return spanner.getDatabaseClient(databaseId);
  }

  private static boolean isEmulatorEndpoint(String endpointUrl) {
    return !endpointUrl.toLowerCase().startsWith("https://");
  }
}
