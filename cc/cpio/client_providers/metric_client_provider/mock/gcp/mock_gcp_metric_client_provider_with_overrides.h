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

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/interface/async_context.h"
#include "cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_provider.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {

class MockGcpMetricClientProviderOverrides : public GcpMetricClientProvider {
 public:
  explicit MockGcpMetricClientProviderOverrides(
      const std::shared_ptr<MetricClientOptions>& metric_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<GcpMetricServiceClientFactory>& factory,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor =
          nullptr)
      : GcpMetricClientProvider(metric_client_options, instance_client_provider,
                                async_executor, factory) {}

  core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept override {
    return GcpMetricClientProvider::MetricsBatchPush(metric_requests_vector);
  }

  void OnAsyncCreateTimeSeriesCallback(
      std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>
          metric_requests_vector,
      google::cloud::future<google::cloud::Status> outcome) noexcept override {
    return GcpMetricClientProvider::OnAsyncCreateTimeSeriesCallback(
        metric_requests_vector, std::move(outcome));
  }
};
}  // namespace google::scp::cpio::client_providers::mock
