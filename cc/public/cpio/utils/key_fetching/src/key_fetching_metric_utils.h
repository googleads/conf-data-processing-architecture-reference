/*
 * Copyright 2026 Google LLC
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

#pragma once

#include <functional>
#include <map>
#include <string>

#include "absl/strings/string_view.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/utils/dual_writing_metric_client/interface/dual_writing_metric_client_interface.h"

namespace google::scp::cpio {
/////// Shared constants ///////
// For metrics labels
constexpr char kErrorCodeLabelName[] = "ErrorCode";
// For metric values
constexpr char kMetricValueOne[] = "1";
constexpr char kDummyLabelValue[] = "null";
///////////////////////////////

/////// Constants for Server startup metrics ///////
// Metric names
constexpr char kPrefetchConfigValidationErrorRateMetricName[] =
    "PrefetchConfigValidationErrorRate";

/////// Constants for request metrics ///////
/// Label names
constexpr char kPrefetchConfigValidationErrorTypeLabelName[] =
    "PrefetchConfigValidationErrorType";

struct PrefetchConfigValidationErrorType {
  static constexpr char kParseFailure[] = "ParseFailure";
  static constexpr char kInvalidDuration[] = "InvalidDuration";
  static constexpr char kEmptyKeyId[] = "EmptyKeyId";
  static constexpr char kMissingKeysetNamespace[] = "MissingKeysetNamespace";
  static constexpr char kDuplicateKeysetNamespace[] =
      "DuplicateKeysetNamespace";
  static constexpr char kMissingValues[] = "MissingValues";
};

struct KeyFetchingErrorType {
  static constexpr char kGenericError[] = "KeyFetchingError";
  static constexpr char kInvalidKeyId[] = "InvalidKeyId";
  static constexpr char kCustomerQuotaExceeded[] = "CustomerQuotaExceeded";
  static constexpr char kCustomerKeyPermissionDenied[] =
      "CustomerKeyPermissionDenied";
};

/////// Constants for KeyFetching metrics ///////
// Metric names
constexpr char kKeyFetchingErrorRateMetricName[] = "KeyFetchingErrorRate";
constexpr char kKeyFetchingRequestRateMetricName[] = "KeyFetchingRequestRate";
constexpr char kKeyFetchingLatencyMetricName[] = "KeyFetchingLatencyInMillis";
constexpr char kKeyCacheStatusMetricName[] = "KeyCacheStatus";
constexpr char kKeyAgeInDaysMetricName[] = "KeyAgeInDaysSinceActivation";

// Metric labels
constexpr char kKeyTypeLabelName[] = "KeyType";
constexpr char kKeyFetchingTypeLabelName[] = "KeyFetchingType";
constexpr char kKeysetNameLabelName[] = "Keyset";
constexpr char kKeyCacheStatusLabelName[] = "KeyCacheStatus";
constexpr char kKeyAgeLabelName[] = "KeyAge";

struct KeyType {
  static constexpr char kEncryptionKey[] = "EncryptionKey";
  static constexpr char kGcpWrappedKey[] = "GcpWrappedKey";
  static constexpr char kAwsWrappedKey[] = "AwsWrappedKey";
  static constexpr char kSidKey[] = "SidKey";
  static constexpr char kKeysetMetadata[] = "KeysetMetadata";
};

struct KeyFetchingType {
  static constexpr char kPrefetch[] = "Prefetch";
  static constexpr char kPrefetchRetry[] = "PrefetchRetry";
  static constexpr char kAutoRefresh[] = "AutoRefresh";
  static constexpr char kOnDemand[] = "OnDemand";
};

struct KeyCacheStatus {
  static constexpr char kValidKeyCacheHit[] = "ValidCacheHit";
  static constexpr char kValidKeyCacheMiss[] = "ValidCacheMiss";
  static constexpr char kInvalidKeyCacheHit[] = "InvalidCacheHit";
};

//////////////////////////////////////////////////

cmrt::sdk::metric_service::v1::Metric CreateMetric(
    absl::string_view metric_name,
    cmrt::sdk::metric_service::v1::MetricType metric_type,
    const std::map<std::string, std::string>& labels,
    absl::string_view metric_value);

void PushPrefetchConfigValidationErrorRateMetric(
    DualWritingMetricClientInterface& metric_client,
    absl::string_view keyset_name, absl::string_view error_type);

std::map<std::string, std::string> CreateKeyFetcherMetricBaseLabels(
    absl::string_view key_type, absl::string_view key_fetching_type,
    absl::string_view keyset_name);

void PushKeyFetchingErrorMetric(DualWritingMetricClientInterface& metric_client,
                                absl::string_view key_type,
                                absl::string_view key_fetching_type,
                                absl::string_view keyset_name,
                                absl::string_view error_string);

void PushWrappedKeyFetchingErrorMetric(
    DualWritingMetricClientInterface& metric_client, absl::string_view key_type,
    absl::string_view error_string);

void PushKeyFetchingRequestMetric(
    DualWritingMetricClientInterface& metric_client, absl::string_view key_type,
    absl::string_view key_fetching_type, absl::string_view keyset_name);

void PushKeyFetchingLatencyMetric(
    DualWritingMetricClientInterface& metric_client, absl::string_view key_type,
    absl::string_view key_fetching_type, absl::string_view keyset_name,
    int64_t latency);

void PushKeyCacheStatusMetric(DualWritingMetricClientInterface& metric_client,
                              absl::string_view key_type,
                              absl::string_view keyset_name,
                              absl::string_view catch_status);

void PushKeyAgeInDaysMetric(DualWritingMetricClientInterface& metric_client,
                            absl::string_view key_type,
                            absl::string_view key_fetching_type,
                            absl::string_view keyset_name,
                            int64_t key_age_in_days);
}  // namespace google::scp::cpio
