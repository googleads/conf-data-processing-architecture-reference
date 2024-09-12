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

package com.google.scp.coordinator.keymanagement.shared.dao.gcp;

import static com.google.scp.shared.gcp.Constants.GCP_TEST_PROJECT_ID;
import static com.google.scp.shared.gcp.Constants.SPANNER_TEST_DB_NAME;
import static com.google.scp.shared.gcp.Constants.SPANNER_TEST_INSTANCE_ID;

import com.google.acai.TestingServiceModule;
import com.google.cloud.spanner.DatabaseClient;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainerTestModule;
import com.google.scp.shared.testutils.gcp.SpannerLocalService;
import java.util.List;

/** Module to setup keydb for spanner. */
public final class SpannerKeyDbTestModule extends AbstractModule {

  public static final SpannerKeyDbConfig TEST_DB_CONFIG =
      SpannerKeyDbConfig.builder()
          .setGcpProjectId(GCP_TEST_PROJECT_ID)
          .setSpannerInstanceId(SPANNER_TEST_INSTANCE_ID)
          .setSpannerDbName(SPANNER_TEST_DB_NAME)
          .setReadStalenessSeconds(0)
          .build();

  public static final List<String> CREATE_TABLE_STATEMENTS =
      List.of(
          "CREATE TABLE KeySets ("
              + "  KeyId STRING(50) NOT NULL,"
              + "  SetName STRING(50) NOT NULL,"
              + "  PublicKey STRING(1000) NOT NULL,"
              + "  PrivateKey STRING(1000) NOT NULL,"
              + "  PublicKeyMaterial STRING(500) NOT NULL,"
              + "  KeySplitData JSON NOT NULL,"
              + "  KeyType STRING(500) NOT NULL,"
              + "  KeyEncryptionKeyUri STRING(1000) NOT NULL,"
              + "  ExpiryTime TIMESTAMP NOT NULL,"
              + "  ActivationTime TIMESTAMP NOT NULL,"
              + "  TtlTime TIMESTAMP NOT NULL,"
              + "  CreatedAt TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp=true),"
              + "  UpdatedAt TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp=true)"
              + ") PRIMARY KEY (KeyId)");

  @Provides
  @Singleton
  @KeyDbClient
  public DatabaseClient getDatabaseClient(DatabaseClient client) {
    return client;
  }

  @Override
  public void configure() {
    bind(KeyDb.class).to(SpannerKeyDb.class);
    bind(SpannerKeyDbConfig.class).toInstance(TEST_DB_CONFIG);
    install(
        new SpannerEmulatorContainerTestModule(
            TEST_DB_CONFIG.gcpProjectId(),
            TEST_DB_CONFIG.spannerInstanceId(),
            TEST_DB_CONFIG.spannerDbName(),
            CREATE_TABLE_STATEMENTS));
    install(TestingServiceModule.forServices(SpannerLocalService.class));
  }
}
