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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/cpio/utils/metric_instance/interface/metric_instance_factory_interface.h"

#include "noop_aggregate_metric.h"
#include "noop_simple_metric.h"

namespace google::scp::cpio {
/**
 * @copydoc MetricInstanceFactoryInterface
 */
class NoopMetricInstanceFactory : public MetricInstanceFactoryInterface {
 public:
  std::unique_ptr<SimpleMetricInterface> ConstructSimpleMetricInstance(
      MetricDefinition metric_info) noexcept override {
    return std::make_unique<NoopSimpleMetric>();
  }

  std::unique_ptr<AggregateMetricInterface> ConstructAggregateMetricInstance(
      MetricDefinition metric_info) noexcept override {
    return std::make_unique<NoopAggregateMetric>();
  }

  std::unique_ptr<AggregateMetricInterface> ConstructAggregateMetricInstance(
      MetricDefinition metric_info,
      const std::vector<std::string>& event_code_labels_list) noexcept
      override {
    return std::make_unique<NoopAggregateMetric>();
  }

  std::unique_ptr<AggregateMetricInterface> ConstructAggregateMetricInstance(
      MetricDefinition metric_info,
      const std::vector<std::string>& event_code_labels_list,
      const std::string& event_code_name) noexcept override {
    return std::make_unique<NoopAggregateMetric>();
  }
};
}  // namespace google::scp::cpio
