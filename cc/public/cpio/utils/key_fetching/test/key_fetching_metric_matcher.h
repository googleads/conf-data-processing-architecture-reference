// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <gtest/gtest.h>

#include <string>

#include "core/test/utils/proto_test_utils.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/utils/dual_writing_metric_client/mock/dual_writing_metric_client_mock.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"

namespace google::scp::cpio {

MATCHER_P4(KeyFetchingErrorMetricEqual, key_type, key_fetching_type,
           keyset_name, error_code, "") {
  auto expected_metric = google::scp::core::test::SubstituteAndParseTextToProto<
      google::cmrt::sdk::metric_service::v1::Metric>(
      R"pb(
        name: "KeyFetchingErrorRate"
        type: METRIC_TYPE_COUNTER
        value: "1"
        labels { key: "KeyType" value: "$0" }
        labels { key: "KeyFetchingType" value: "$1" }
        labels { key: "Keyset" value: "$2" }
        labels { key: "ErrorCode" value: "$3" }
      )pb",
      key_type, key_fetching_type, keyset_name, error_code);
  return testing::ExplainMatchResult(
      google::scp::core::test::EqualsUnorderedProto(expected_metric), arg,
      result_listener);
}

MATCHER_P3(KeyFetchingRequestMetricEqual, key_type, key_fetching_type,
           keyset_name, "") {
  auto expected_metric = google::scp::core::test::SubstituteAndParseTextToProto<
      google::cmrt::sdk::metric_service::v1::Metric>(
      R"pb(
        name: "KeyFetchingRequestRate"
        type: METRIC_TYPE_COUNTER
        value: "1"
        labels { key: "KeyType" value: "$0" }
        labels { key: "KeyFetchingType" value: "$1" }
        labels { key: "Keyset" value: "$2" }
      )pb",
      key_type, key_fetching_type, keyset_name);
  return testing::ExplainMatchResult(
      google::scp::core::test::EqualsUnorderedProto(expected_metric), arg,
      result_listener);
}

MATCHER_P4(KeyFetchingLatencyMetricEqual, key_type, key_fetching_type,
           keyset_name, value, "") {
  auto expected_metric = google::scp::core::test::SubstituteAndParseTextToProto<
      google::cmrt::sdk::metric_service::v1::Metric>(
      R"pb(
        name: "KeyFetchingLatencyInMillis"
        type: METRIC_TYPE_HISTOGRAM
        labels { key: "KeyType" value: "$0" }
        labels { key: "KeyFetchingType" value: "$1" }
        labels { key: "Keyset" value: "$2" }
      )pb",
      key_type, key_fetching_type, keyset_name);
  auto may_clear_value = arg;
  if (std::string(value).empty()) {
    may_clear_value.clear_value();
  } else {
    expected_metric.set_value(value);
  }
  return testing::ExplainMatchResult(
      google::scp::core::test::EqualsUnorderedProto(expected_metric),
      may_clear_value, result_listener);
}

MATCHER_P3(KeyCacheStatusMetricEqual, key_type, keyset_name, key_cache_status,
           "") {
  auto expected_metric = google::scp::core::test::SubstituteAndParseTextToProto<
      google::cmrt::sdk::metric_service::v1::Metric>(
      R"pb(
        name: "KeyCacheStatus"
        type: METRIC_TYPE_COUNTER
        value: "1"
        labels { key: "KeyType" value: "$0" }
        labels { key: "Keyset" value: "$1" }
        labels { key: "KeyCacheStatus" value: "$2" }
      )pb",
      key_type, keyset_name, key_cache_status);
  return testing::ExplainMatchResult(
      google::scp::core::test::EqualsUnorderedProto(expected_metric), arg,
      result_listener);
}

MATCHER_P4(KeyAgeInDaysMetricEqual, key_type, key_fetching_type, keyset_name,
           key_age_in_days, "") {
  auto expected_metric = google::scp::core::test::SubstituteAndParseTextToProto<
      google::cmrt::sdk::metric_service::v1::Metric>(
      R"pb(
        name: "KeyAgeInDaysSinceActivation"
        type: METRIC_TYPE_GAUGE
        labels { key: "KeyType" value: "$0" }
        labels { key: "KeyFetchingType" value: "$1" }
        labels { key: "Keyset" value: "$2" }
      )pb",
      key_type, key_fetching_type, keyset_name);
  auto may_clear_value = arg;
  if (std::string(key_age_in_days).empty()) {
    may_clear_value.clear_value();
  } else {
    expected_metric.set_value(key_age_in_days);
  }
  return testing::ExplainMatchResult(
      google::scp::core::test::EqualsUnorderedProto(expected_metric),
      may_clear_value, result_listener);
}

// call_count < 0 means the times is not deterministic.
void ExpectOtelKeyFetchingErrorMetricPush(
    DualWritingMetricClientMock& mock_metric_client, int call_count,
    absl::string_view key_type = "", absl::string_view key_fetching_type = "",
    absl::string_view keyset_name = "",
    absl::string_view error_code = KeyFetchingErrorType::kGenericError);

// call_count < 0 means the times is not deterministic.
void ExpectOtelKeyFetchingRequestMetricPush(
    DualWritingMetricClientMock& mock_metric_client, int call_count,
    absl::string_view key_type = "", absl::string_view key_fetching_type = "",
    absl::string_view keyset_name = "");

// call_count < 0 means the times is not deterministic.
void ExpectOtelKeyFetchingLatencyMetricPush(
    DualWritingMetricClientMock& mock_metric_client, int call_count,
    absl::string_view key_type = "", absl::string_view key_fetching_type = "",
    absl::string_view keyset_name = "");

// call_count < 0 means the times is not deterministic.
void ExpectOtelKeyCacheStatusMetricPush(
    DualWritingMetricClientMock& mock_metric_client, int call_count,
    absl::string_view key_type = "", absl::string_view keyset_name = "",
    absl::string_view key_cache_status = KeyCacheStatus::kValidKeyCacheHit);

// call_count < 0 means the times is not deterministic.
void ExpectOtelKeyAgeInDaysMetricPush(
    DualWritingMetricClientMock& mock_metric_client, int call_count,
    absl::string_view key_type = "", absl::string_view key_fetching_type = "",
    absl::string_view keyset_name = "", int16_t key_age_in_days = 0);

}  // namespace google::scp::cpio
