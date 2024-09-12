/*
 * Copyright 2024 Google LLC
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

#include "fake_gcp_metric_client_provider.h"

#include <memory>
#include <string>

#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_provider.h"
#include "google/cloud/monitoring/mocks/mock_metric_connection.h"

using google::cloud::make_ready_future;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::monitoring::MetricServiceClient;
using google::cloud::monitoring_mocks::MockMetricServiceConnection;
using google::monitoring::v3::CreateTimeSeriesRequest;
using google::scp::core::AsyncExecutorInterface;
using std::make_shared;
using std::shared_ptr;
using std::string;
using testing::NiceMock;
using testing::Return;

namespace google::scp::cpio::client_providers {
std::shared_ptr<MetricServiceClient>
FakeGcpMetricServiceClientFactory::CreateClient() noexcept {
  auto connection = make_shared<NiceMock<MockMetricServiceConnection>>();
  auto mock_client = make_shared<MetricServiceClient>(connection);
  ON_CALL(*connection, AsyncCreateTimeSeries)
      .WillByDefault([&](CreateTimeSeriesRequest const& request) {
        return make_ready_future(Status(StatusCode::kOk, ""));
      });
  return mock_client;
}

shared_ptr<MetricClientInterface> MetricClientProviderFactory::Create(
    const shared_ptr<MetricClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpMetricClientProvider>(
      options, instance_client_provider, async_executor,
      make_shared<FakeGcpMetricServiceClientFactory>());
}
}  // namespace google::scp::cpio::client_providers
