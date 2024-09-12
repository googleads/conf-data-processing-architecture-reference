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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/cpio/mock/auto_scaling_client/mock_auto_scaling_client.h"
#include "public/cpio/mock/blob_storage_client/mock_blob_storage_client.h"
#include "public/cpio/mock/crypto_client/mock_crypto_client.h"
#include "public/cpio/mock/instance_client/mock_instance_client.h"
#include "public/cpio/mock/job_client/mock_job_client.h"
#include "public/cpio/mock/kms_client/mock_kms_client.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"
#include "public/cpio/mock/nosql_database_client/mock_nosql_database_client.h"
#include "public/cpio/mock/parameter_client/mock_parameter_client.h"
#include "public/cpio/mock/private_key_client/mock_private_key_client.h"
#include "public/cpio/mock/public_key_client/mock_public_key_client.h"
#include "public/cpio/mock/queue_client/mock_queue_client.h"
#include "public/cpio/utils/configuration_fetcher/mock/mock_configuration_fetcher.h"
#include "public/cpio/utils/job_lifecycle_helper/mock/mock_job_lifecycle_helper.h"
#include "public/cpio/utils/metric_instance/mock/mock_metric_instance_factory.h"

namespace google::scp::cpio {
TEST(PublicMocksTest, CreateMocks) {
  MockAutoScalingClient auto_scaling_client;
  MockBlobStorageClient blob_storage_client;
  MockCryptoClient crypto_client;
  MockInstanceClient instance_client;
  MockJobClient job_client;
  MockKmsClient kms_client;
  MockMetricClient metric_client;
  MockNoSQLDatabaseClient nosql_database_client;
  MockParameterClient parameter_client;
  MockPrivateKeyClient private_key_client;
  MockPublicKeyClient public_key_client;
  MockQueueClient queue_client;
  MockConfigurationFetcher configuration_fetcher;
  MockJobLifecycleHelper job_lifecycle_helper;
  MockMetricInstanceFactory metric_instance_factory;
  MockSimpleMetric simple_metric;
  MockAggregateMetric aggregate_metric;

  std::vector<std::string> event_codes;
  std::map<std::string, std::string> metric_labels;
  MetricDefinition metric_definition(
      "a", cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_BITS,
      std::nullopt, metric_labels);
  EXPECT_TRUE(metric_instance_factory.ConstructAggregateMetricInstance(
      metric_definition));
  EXPECT_TRUE(metric_instance_factory.ConstructAggregateMetricInstance(
      metric_definition, event_codes));
  EXPECT_TRUE(metric_instance_factory.ConstructAggregateMetricInstance(
      metric_definition, event_codes, "name"));
  EXPECT_TRUE(
      metric_instance_factory.ConstructSimpleMetricInstance(metric_definition));
}
}  // namespace google::scp::cpio
