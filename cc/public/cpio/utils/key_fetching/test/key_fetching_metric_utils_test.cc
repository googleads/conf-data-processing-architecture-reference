/*
 * Copyright 2025 Google LLC
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

#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <string>

#include "core/test/utils/proto_test_utils.h"
#include "cpio/common/src/common_error_codes.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/utils/dual_writing_metric_client/mock/dual_writing_metric_client_mock.h"

#include "key_fetching_metric_matcher.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::MetricType;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CPIO_ENTITY_NOT_FOUND;
using google::scp::core::test::EqualsUnorderedProto;
using google::scp::core::test::SubstituteAndParseTextToProto;
using std::map;
using std::string;
using testing::NiceMock;
using ::testing::Pair;
using testing::Return;
using ::testing::UnorderedElementsAre;

namespace google::scp::cpio {

TEST(MetricUtilsTest, CreateMetricTest) {
  static string metric_name = "test_metric";
  static MetricType metric_type = MetricType::METRIC_TYPE_COUNTER;
  static string metric_value = "1";
  map<string, string> labels = {{"label1", "a"}, {"label2", "b"}};
  auto expected_metric = SubstituteAndParseTextToProto<Metric>(
      R"pb(
        name: "test_metric"
        type: METRIC_TYPE_COUNTER
        value: "1"
        labels { key: "label1" value: "a" }
        labels { key: "label2" value: "b" }
      )pb");

  EXPECT_THAT(CreateMetric(metric_name, metric_type, labels, metric_value),
              EqualsUnorderedProto(expected_metric));
}

TEST(MetricUtilsTest, CreateKeyFetcherMetricBaseLabelsTest) {
  EXPECT_THAT(
      CreateKeyFetcherMetricBaseLabels(KeyType::kEncryptionKey,
                                       KeyFetchingType::kAutoRefresh, "test"),
      UnorderedElementsAre(Pair("KeyType", "EncryptionKey"),
                           Pair("KeyFetchingType", "AutoRefresh"),
                           Pair("Keyset", "test")));
}

TEST(MetricUtilsTest, PushKeyFetchingErrorMetricTest) {
  NiceMock<DualWritingMetricClientMock> mock_metric_client;
  auto keyset_name = "test";
  auto key_type = KeyType::kSidKey;
  auto key_fetching_type = KeyFetchingType::kAutoRefresh;
  EXPECT_CALL(mock_metric_client, PutOtelMetric(KeyFetchingErrorMetricEqual(
                                      key_type, key_fetching_type, keyset_name,
                                      KeyFetchingErrorType::kInvalidKeyId)))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyFetchingErrorMetric(mock_metric_client, key_type, key_fetching_type,
                             keyset_name, KeyFetchingErrorType::kInvalidKeyId);

  key_type = KeyType::kEncryptionKey;
  key_fetching_type = KeyFetchingType::kOnDemand;
  EXPECT_CALL(mock_metric_client, PutOtelMetric(KeyFetchingErrorMetricEqual(
                                      key_type, key_fetching_type, keyset_name,
                                      KeyFetchingErrorType::kGenericError)))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyFetchingErrorMetric(mock_metric_client, key_type, key_fetching_type,
                             keyset_name, KeyFetchingErrorType::kGenericError);
}

TEST(MetricUtilsTest, PushKeyFetchingRequestMetricTest) {
  NiceMock<DualWritingMetricClientMock> mock_metric_client;
  auto keyset_name = "test";
  auto key_type = KeyType::kSidKey;
  auto key_fetching_type = KeyFetchingType::kAutoRefresh;
  EXPECT_CALL(mock_metric_client,
              PutOtelMetric(KeyFetchingRequestMetricEqual(
                  key_type, key_fetching_type, keyset_name)))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyFetchingRequestMetric(mock_metric_client, key_type, key_fetching_type,
                               keyset_name);

  key_type = KeyType::kEncryptionKey;
  key_fetching_type = KeyFetchingType::kOnDemand;
  EXPECT_CALL(mock_metric_client,
              PutOtelMetric(KeyFetchingRequestMetricEqual(
                  key_type, key_fetching_type, keyset_name)))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyFetchingRequestMetric(mock_metric_client, key_type, key_fetching_type,
                               keyset_name);
}

TEST(MetricUtilsTest, PushKeyFetchingLatencyMetricTest) {
  NiceMock<DualWritingMetricClientMock> mock_metric_client;
  auto keyset_name = "test";
  auto key_type = KeyType::kSidKey;
  auto key_fetching_type = KeyFetchingType::kAutoRefresh;
  int64_t value = 1000;
  EXPECT_CALL(mock_metric_client,
              PutOtelMetric(KeyFetchingLatencyMetricEqual(
                  key_type, key_fetching_type, keyset_name, "1000")))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyFetchingLatencyMetric(mock_metric_client, key_type, key_fetching_type,
                               keyset_name, value);

  key_type = KeyType::kEncryptionKey;
  key_fetching_type = KeyFetchingType::kOnDemand;
  EXPECT_CALL(mock_metric_client,
              PutOtelMetric(KeyFetchingLatencyMetricEqual(
                  key_type, key_fetching_type, keyset_name, "1000")))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyFetchingLatencyMetric(mock_metric_client, key_type, key_fetching_type,
                               keyset_name, value);
}

TEST(MetricUtilsTest, PushKeyCacheStatusMetricTest) {
  NiceMock<DualWritingMetricClientMock> mock_metric_client;
  auto keyset_name = "test";
  auto key_type = KeyType::kSidKey;
  auto key_cache_status = KeyCacheStatus::kValidKeyCacheHit;
  EXPECT_CALL(mock_metric_client, PutOtelMetric(KeyCacheStatusMetricEqual(
                                      key_type, keyset_name, key_cache_status)))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyCacheStatusMetric(mock_metric_client, key_type, keyset_name,
                           key_cache_status);

  key_type = KeyType::kEncryptionKey;
  key_cache_status = KeyCacheStatus::kValidKeyCacheMiss;
  EXPECT_CALL(mock_metric_client, PutOtelMetric(KeyCacheStatusMetricEqual(
                                      key_type, keyset_name, key_cache_status)))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyCacheStatusMetric(mock_metric_client, key_type, keyset_name,
                           key_cache_status);
}

TEST(MetricUtilsTest, PushKeyAgeInDaysMetricTest) {
  NiceMock<DualWritingMetricClientMock> mock_metric_client;
  auto keyset_name = "test";
  auto key_type = KeyType::kSidKey;
  auto key_fetching_type = KeyFetchingType::kOnDemand;
  auto age_in_days = 1;
  EXPECT_CALL(mock_metric_client,
              PutOtelMetric(KeyAgeInDaysMetricEqual(key_type, key_fetching_type,
                                                    keyset_name, "1")))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyAgeInDaysMetric(mock_metric_client, key_type, key_fetching_type,
                         keyset_name, age_in_days);

  key_type = KeyType::kEncryptionKey;
  key_fetching_type = KeyFetchingType::kPrefetch;
  age_in_days = 0;
  EXPECT_CALL(mock_metric_client,
              PutOtelMetric(KeyAgeInDaysMetricEqual(key_type, key_fetching_type,
                                                    keyset_name, "0")))
      .WillOnce(Return(SuccessExecutionResult()));

  PushKeyAgeInDaysMetric(mock_metric_client, key_type, key_fetching_type,
                         keyset_name, age_in_days);
}

}  // namespace google::scp::cpio
