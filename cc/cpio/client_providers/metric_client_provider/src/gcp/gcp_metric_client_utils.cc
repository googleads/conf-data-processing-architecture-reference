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

#include "gcp_metric_client_utils.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/util/time_util.h>

#include "core/interface/async_context.h"
#include "google/cloud/future.h"
#include "google/cloud/monitoring/metric_client.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"

using google::api::MonitoredResource;
using google::cloud::future;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::monitoring::v3::TimeSeries;
using google::protobuf::MapPair;
using google::protobuf::Timestamp;
using google::protobuf::util::MessageDifferencer;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_DUPLICATE_METRIC_IN_ONE_REQUEST;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_FAILED_OVERSIZE_METRIC_LABELS;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP;
using google::scp::core::errors::SC_GCP_METRIC_CLIENT_INVALID_METRIC_LABEL_KEY;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_INVALID_METRIC_LABEL_VALUE;
using google::scp::core::errors::SC_GCP_METRIC_CLIENT_INVALID_METRIC_VALUE;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_TOO_MANY_METRICS_IN_ONE_REQUEST;
using std::shared_ptr;
using std::stod;
using std::string;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::minutes;
using std::chrono::seconds;

namespace {
/// Prefix of the metric type for all custom metrics.
constexpr char kCustomMetricTypePrefix[] = "custom.googleapis.com";
/// Prefix of project name.
constexpr char kProjectNamePrefix[] = "projects/";
constexpr char kResourceType[] = "gce_instance";
constexpr char kProjectIdKey[] = "project_id";
constexpr char kInstanceIdKey[] = "instance_id";
constexpr char kInstanceZoneKey[] = "zone";
// Limitation for GCP user-defined metrics. For more information, please see
// https://cloud.google.com/monitoring/quotas#custom_metrics_quotas
constexpr size_t kGcpMetricLabelsSizeLimit = 30;
constexpr size_t kGcpStringLengthForLabelKey = 100;
constexpr size_t kGcpStringLengthForLabelValue = 1024;
constexpr size_t kGcpTimeSeriesInOneRequest = 200;
constexpr int k25HoursSecondsCount = duration_cast<seconds>(hours(25)).count();
constexpr int k5MinsSecondsCount = duration_cast<seconds>(minutes(5)).count();

ExecutionResult ValidateMetricLabels(const Metric& metric) {
  // Check labels size.
  if (metric.labels().size() > kGcpMetricLabelsSizeLimit) {
    return FailureExecutionResult(
        SC_GCP_METRIC_CLIENT_FAILED_OVERSIZE_METRIC_LABELS);
  }

  // Check label key and value string length.
  for (const auto& [label_key, label_value] : metric.labels()) {
    if (label_key.length() > kGcpStringLengthForLabelKey) {
      return FailureExecutionResult(
          SC_GCP_METRIC_CLIENT_INVALID_METRIC_LABEL_KEY);
    }
    if (label_value.length() > kGcpStringLengthForLabelValue) {
      return FailureExecutionResult(
          SC_GCP_METRIC_CLIENT_INVALID_METRIC_LABEL_VALUE);
    }
  }

  return SuccessExecutionResult();
}

ExecutionResult ValidateMetricTimestamp(const Metric& metric) {
  auto current_time = TimeUtil::GetCurrentTime();
  auto difference =
      TimeUtil::DurationToSeconds(current_time - metric.timestamp());

  // A valid timestamp of gcp custom metric cannot be earlier than 25
  // hours or later than 5 mins.
  if (difference > k25HoursSecondsCount || difference < -k5MinsSecondsCount) {
    return FailureExecutionResult(
        SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP);
  }

  return SuccessExecutionResult();
}

ExecutionResult VerifyNoDuplicateMetrics(
    const vector<TimeSeries>& time_series_list,
    const TimeSeries& pending_time_series) {
  // Check pending_time_series didn't appear in time_series_list.
  for (const auto& time_series : time_series_list) {
    if (MessageDifferencer::Equals(time_series.metric(),
                                   pending_time_series.metric())) {
      return FailureExecutionResult(
          SC_GCP_METRIC_CLIENT_DUPLICATE_METRIC_IN_ONE_REQUEST);
    }
  }

  return SuccessExecutionResult();
}

}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResultOr<vector<TimeSeries>>
GcpMetricClientUtils::ParseRequestToTimeSeries(
    const shared_ptr<PutMetricsRequest>& put_metric_request,
    const string& name_space) noexcept {
  vector<TimeSeries> time_series_list;
  if (put_metric_request->metrics().size() > kGcpTimeSeriesInOneRequest) {
    return FailureExecutionResult(
        SC_GCP_METRIC_CLIENT_TOO_MANY_METRICS_IN_ONE_REQUEST);
  }

  for (auto i = 0; i < put_metric_request->metrics().size(); ++i) {
    const auto& metric = put_metric_request->metrics()[i];

    auto execution_result = ValidateMetricLabels(metric);
    RETURN_IF_FAILURE(execution_result);

    TimeSeries time_series;
    time_series.mutable_metric()->mutable_labels()->insert(
        metric.labels().begin(), metric.labels().end());
    time_series.mutable_metric()->set_type(string(kCustomMetricTypePrefix) +
                                           "/" + name_space + "/" +
                                           metric.name());

    auto* point = time_series.add_points();
    try {
      point->mutable_value()->set_double_value(stod(metric.value()));
    } catch (...) {
      return FailureExecutionResult(SC_GCP_METRIC_CLIENT_INVALID_METRIC_VALUE);
    }

    auto timestamp = TimeUtil::GetCurrentTime();
    if (metric.timestamp().seconds() != 0 || metric.timestamp().nanos() != 0) {
      RETURN_IF_FAILURE(ValidateMetricTimestamp(metric));
      timestamp = metric.timestamp();
    }
    point->mutable_interval()->mutable_end_time()->CopyFrom(timestamp);

    execution_result = VerifyNoDuplicateMetrics(time_series_list, time_series);
    RETURN_IF_FAILURE(execution_result);

    time_series_list.emplace_back(time_series);
  }

  return time_series_list;
}

string GcpMetricClientUtils::ConstructProjectName(const string& project_id) {
  return string(kProjectNamePrefix) + project_id;
}

void GcpMetricClientUtils::AddResourceToTimeSeries(
    const string& project_id, const string& instance_id,
    const string& instance_zone,
    vector<TimeSeries>& time_series_list) noexcept {
  MonitoredResource resource;
  resource.set_type(kResourceType);
  auto labels = resource.mutable_labels();
  labels->insert(MapPair<string, string>(string(kProjectIdKey), project_id));
  labels->insert(MapPair<string, string>(string(kInstanceIdKey), instance_id));
  labels->insert(
      MapPair<string, string>(string(kInstanceZoneKey), instance_zone));

  for (auto& time_series : time_series_list) {
    time_series.mutable_resource()->CopyFrom(resource);
  }
}

}  // namespace google::scp::cpio::client_providers
