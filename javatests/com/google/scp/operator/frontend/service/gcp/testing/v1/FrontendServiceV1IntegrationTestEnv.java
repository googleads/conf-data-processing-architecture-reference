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

package com.google.scp.operator.frontend.service.gcp.testing.v1;

import com.google.cloud.spanner.DatabaseClient;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.frontend.service.gcp.testing.FrontendServiceIntegrationTestModule;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataDbClient;
import com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerMetadataDbTestModule;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainer;

/** Test environment for frontend service integration test */
public class FrontendServiceV1IntegrationTestEnv extends AbstractModule {
  private Injector spannerInjector;

  @Provides
  @Singleton
  public SpannerEmulatorContainer provideSpannerEmulatorContainer() {
    return spannerInjector.getInstance(SpannerEmulatorContainer.class);
  }

  @Override
  public void configure() {
    spannerInjector = Guice.createInjector(new SpannerMetadataDbTestModule());
    spannerInjector.getInstance(SpannerEmulatorContainer.class).start();

    bind(DatabaseClient.class)
        .annotatedWith(JobMetadataDbClient.class)
        .toInstance(spannerInjector.getInstance(DatabaseClient.class));

    install(new FrontendServiceIntegrationTestModule());
    install(new FrontendServiceV1CloudFunctionEmulatorContainer());
  }
}
