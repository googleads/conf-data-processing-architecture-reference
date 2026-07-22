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

#include "key_fetching_metric_utils.h"

#include <functional>
#include <map>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "core/common/global_logger/src/global_logger.h"
#include "public/core/interface/execution_result_macros.h"
#include "public/cpio/interface/error_codes.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::MetricType;
using google::scp::core::ExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_CPIO_ENTITY_NOT_FOUND;
using google::scp::core::errors::SC_CPIO_INVALID_ARGUMENT;
using std::map;
using std::string;

namespace google::scp::cpio {
namespace {
constexpr char kMetricUtilsComponentName[] = "DualWritingMetricUtils";

map<string, string> CreateKeyMetricBaseLabels(absl::string_view key_type,
                                              absl::string_view keyset_name) {
  map<string, string> labels;
  labels[kKeysetNameLabelName] = keyset_name;
  labels[kKeyTypeLabelName] = key_type;
  return labels;
}
}  // namespace

Metric CreateMetric(absl::string_view metric_name, MetricType metric_type,
                    const map<string, string>& labels,
                    absl::string_view metric_value) {
  Metric metric;
  metric.set_name(metric_name);
  metric.set_type(metric_type);
  metric.set_value(metric_value);
  *metric.mutable_labels() = {labels.begin(), labels.end()};
  return metric;
}

void PushPrefetchConfigValidationErrorRateMetric(
    DualWritingMetricClientInterface& metric_client,
    absl::string_view keyset_name, absl::string_view error_type) {
  map<string, string> labels;
  labels[kKeysetNameLabelName] =
      keyset_name.empty() ? kDummyLabelValue : keyset_name;
  labels[kPrefetchConfigValidationErrorTypeLabelName] = error_type;
  auto metric =
      CreateMetric(kPrefetchConfigValidationErrorRateMetricName,
                   MetricType::METRIC_TYPE_COUNTER, labels, kMetricValueOne);
  LOG_IF_FAILURE(metric_client.PutOtelMetric(metric), kMetricUtilsComponentName,
                 kZeroUuid,
                 "Failed to put PrefetchConfigValidationErrorRate metric");
}

map<string, string> CreateKeyFetcherMetricBaseLabels(
    absl::string_view key_type, absl::string_view key_fetching_type,
    absl::string_view keyset_name) {
  auto labels = CreateKeyMetricBaseLabels(key_type, keyset_name);
  labels[kKeyFetchingTypeLabelName] = key_fetching_type;
  return labels;
}

void PushKeyFetchingErrorMetric(DualWritingMetricClientInterface& metric_client,
                                absl::string_view key_type,
                                absl::string_view key_fetching_type,
                                absl::string_view keyset_name,
                                absl::string_view error_string) {
  auto labels = CreateKeyFetcherMetricBaseLabels(key_type, key_fetching_type,
                                                 keyset_name);
  labels[kErrorCodeLabelName] = error_string;
  auto metric =
      CreateMetric(kKeyFetchingErrorRateMetricName,
                   MetricType::METRIC_TYPE_COUNTER, labels, kMetricValueOne);
  LOG_IF_FAILURE(metric_client.PutOtelMetric(metric), kMetricUtilsComponentName,
                 kZeroUuid, "Failed to put KeyFetchingErrorRate metric");
}

void PushWrappedKeyFetchingErrorMetric(
    DualWritingMetricClientInterface& metric_client, absl::string_view key_type,
    absl::string_view error_string) {
  auto labels =
      CreateKeyFetcherMetricBaseLabels(key_type, KeyFetchingType::kOnDemand,
                                       /*keyset_name=*/kDummyLabelValue);
  labels[kErrorCodeLabelName] = error_string;
  auto metric =
      CreateMetric(kKeyFetchingErrorRateMetricName,
                   MetricType::METRIC_TYPE_COUNTER, labels, kMetricValueOne);
  LOG_IF_FAILURE(metric_client.PutOtelMetric(metric), kMetricUtilsComponentName,
                 kZeroUuid, "Failed to put KeyFetchingErrorRate metric");
}

void PushKeyFetchingRequestMetric(
    DualWritingMetricClientInterface& metric_client, absl::string_view key_type,
    absl::string_view key_fetching_type, absl::string_view keyset_name) {
  auto labels = CreateKeyFetcherMetricBaseLabels(key_type, key_fetching_type,
                                                 keyset_name);
  auto metric =
      CreateMetric(kKeyFetchingRequestRateMetricName,
                   MetricType::METRIC_TYPE_COUNTER, labels, kMetricValueOne);
  LOG_IF_FAILURE(metric_client.PutOtelMetric(metric), kMetricUtilsComponentName,
                 kZeroUuid, "Failed to put KeyFetchingRequestRate metric");
}

void PushKeyFetchingLatencyMetric(
    DualWritingMetricClientInterface& metric_client, absl::string_view key_type,
    absl::string_view key_fetching_type, absl::string_view keyset_name,
    int64_t latency) {
  auto labels = CreateKeyFetcherMetricBaseLabels(key_type, key_fetching_type,
                                                 keyset_name);
  auto metric = CreateMetric(kKeyFetchingLatencyMetricName,
                             MetricType::METRIC_TYPE_HISTOGRAM, labels,
                             absl::StrCat(latency));
  LOG_IF_FAILURE(metric_client.PutOtelMetric(metric), kMetricUtilsComponentName,
                 kZeroUuid, "Failed to put KeyFetchingLatencyInMillis metric");
}

void PushKeyCacheStatusMetric(DualWritingMetricClientInterface& metric_client,
                              absl::string_view key_type,
                              absl::string_view keyset_name,
                              absl::string_view cache_status) {
  auto labels = CreateKeyMetricBaseLabels(key_type, keyset_name);
  labels[kKeyCacheStatusLabelName] = cache_status;
  auto metric =
      CreateMetric(kKeyCacheStatusMetricName, MetricType::METRIC_TYPE_COUNTER,
                   labels, kMetricValueOne);
  LOG_IF_FAILURE(metric_client.PutOtelMetric(metric), kMetricUtilsComponentName,
                 kZeroUuid, "Failed to put KeyCacheStatus metric");
}

void PushKeyAgeInDaysMetric(DualWritingMetricClientInterface& metric_client,
                            absl::string_view key_type,
                            absl::string_view key_fetching_type,
                            absl::string_view keyset_name,
                            int64_t key_age_in_days) {
  auto labels = CreateKeyFetcherMetricBaseLabels(key_type, key_fetching_type,
                                                 keyset_name);
  auto metric =
      CreateMetric(kKeyAgeInDaysMetricName, MetricType::METRIC_TYPE_GAUGE,
                   labels, absl::StrCat(key_age_in_days));
  LOG_IF_FAILURE(metric_client.PutOtelMetric(metric), kMetricUtilsComponentName,
                 kZeroUuid, "Failed to put KeyAgeInDays metric");
}
}  // namespace google::scp::cpio
