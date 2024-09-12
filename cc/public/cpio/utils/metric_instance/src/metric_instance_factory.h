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

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/interface/async_executor_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_instance/interface/metric_instance_factory_interface.h"

namespace google::scp::cpio {

/// The default aggregate interval in milliseconds for AggregatedMetric. GCP
/// user-defined metric quota limits only allow one data point per metric per 5
/// seconds. Therefore, the AggregateMetric aggregate time interval must be set
/// to at least 5 seconds. For more information, please see
/// https://cloud.google.com/monitoring/quotas#custom_metrics_quotas
static constexpr core::TimeDuration kDefaultAggregatedMetricIntervalMs = 5000;

/**
 * @copydoc MetricInstanceFactoryInterface
 */
class MetricInstanceFactory : public MetricInstanceFactoryInterface {
 public:
  explicit MetricInstanceFactory(
      core::AsyncExecutorInterface* async_executor,
      MetricClientInterface* metric_client,
      core::TimeDuration aggregated_metric_interval_ms =
          kDefaultAggregatedMetricIntervalMs);

  std::unique_ptr<SimpleMetricInterface> ConstructSimpleMetricInstance(
      MetricDefinition metric_info) noexcept override;

  std::unique_ptr<AggregateMetricInterface> ConstructAggregateMetricInstance(
      MetricDefinition metric_info) noexcept override;

  std::unique_ptr<AggregateMetricInterface> ConstructAggregateMetricInstance(
      MetricDefinition metric_info,
      const std::vector<std::string>& event_code_labels_list) noexcept override;

  std::unique_ptr<AggregateMetricInterface> ConstructAggregateMetricInstance(
      MetricDefinition metric_info,
      const std::vector<std::string>& event_code_labels_list,
      const std::string& event_code_name) noexcept override;

  /// An instance to the async executor.
  core::AsyncExecutorInterface* async_executor_;
  /// Metric client instance.
  MetricClientInterface* metric_client_;
  /// The time interval in milliseconds that the AggregateMetric aggregates
  /// metrics and pushes their values to the cloud.
  core::TimeDuration aggregated_metric_interval_ms_;
};
}  // namespace google::scp::cpio
