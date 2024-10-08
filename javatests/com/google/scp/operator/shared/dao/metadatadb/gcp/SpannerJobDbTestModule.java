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

import static com.google.scp.shared.gcp.Constants.GCP_TEST_PROJECT_ID;
import static com.google.scp.shared.gcp.Constants.SPANNER_TEST_DB_NAME;
import static com.google.scp.shared.gcp.Constants.SPANNER_TEST_INSTANCE_ID;
import static com.google.scp.shared.gcp.Constants.SPANNER_TEST_JOB_DB_NAME;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.acai.TestingServiceModule;
import com.google.cloud.spanner.DatabaseClient;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb.JobDbClient;
import com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerJobDb.JobDbSpannerTtlDays;
import com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerJobDb.JobDbTableName;
import com.google.scp.shared.mapper.TimeObjectMapper;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainerTestModule;
import com.google.scp.shared.testutils.gcp.SpannerLocalService;
import java.util.List;

public final class SpannerJobDbTestModule extends AbstractModule {

  public static final List<String> CREATE_TABLE_STATEMENTS =
      List.of(
          "CREATE TABLE "
              + SPANNER_TEST_JOB_DB_NAME
              + " ("
              + "JobId STRING(256) NOT NULL,"
              + "Value JSON NOT NULL,"
              + "Ttl TIMESTAMP NOT NULL,"
              + ") PRIMARY KEY (JobId)");

  @Provides
  @Singleton
  @JobDbClient
  public DatabaseClient getDatabaseClient(DatabaseClient client) {
    return client;
  }

  @Override
  public void configure() {
    bind(ObjectMapper.class).toInstance(new TimeObjectMapper());
    install(
        new SpannerEmulatorContainerTestModule(
            GCP_TEST_PROJECT_ID,
            SPANNER_TEST_INSTANCE_ID,
            SPANNER_TEST_DB_NAME,
            CREATE_TABLE_STATEMENTS));
    bind(String.class).annotatedWith(JobDbTableName.class).toInstance(SPANNER_TEST_JOB_DB_NAME);
    bind(String.class).annotatedWith(JobDbSpannerTtlDays.class).toInstance("365");
    install(TestingServiceModule.forServices(SpannerLocalService.class));
  }
}
