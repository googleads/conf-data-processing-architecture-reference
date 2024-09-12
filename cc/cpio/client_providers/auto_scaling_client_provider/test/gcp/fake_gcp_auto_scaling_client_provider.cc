
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

#include "fake_gcp_auto_scaling_client_provider.h"

#include <memory>
#include <string>

#include "cpio/client_providers/instance_database_client_provider/mock/mock_instance_database_client_provider.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "google/cloud/compute/region_instance_group_managers/v1/mocks/mock_region_instance_group_managers_connection.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/auto_scaling_client/type_def.h"

using google::cloud::make_ready_future;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::StatusOr;
using google::cloud::compute_region_instance_group_managers_v1::
    RegionInstanceGroupManagersClient;
using google::cloud::compute_region_instance_group_managers_v1_mocks::
    MockRegionInstanceGroupManagersConnection;
using google::cloud::cpp::compute::region_instance_group_managers::v1::
    DeleteInstancesRequest;
using google::cloud::cpp::compute::v1::Operation;
using google::cmrt::sdk::instance_database_client::GetInstanceByNameRequest;
using google::cmrt::sdk::instance_database_client::GetInstanceByNameResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::client_providers::mock::
    MockInstanceDatabaseClientProvider;
using google::scp::operators::protos::shared::backend::asginstance::
    InstanceStatus;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;
using testing::NiceMock;

namespace google::scp::cpio::client_providers {
shared_ptr<RegionInstanceGroupManagersClient>
FakeInstanceGroupManagersClientFactory::CreateClient(
    const std::shared_ptr<AutoScalingClientOptions>& options) noexcept {
  auto connection =
      make_shared<NiceMock<MockRegionInstanceGroupManagersConnection>>();
  auto client = make_shared<RegionInstanceGroupManagersClient>(connection);
  ON_CALL(*connection, DeleteInstances)
      .WillByDefault([&](DeleteInstancesRequest const& request) {
        Operation op;
        op.set_name("delete-instances");
        return make_ready_future(StatusOr<Operation>(op));
      });
  return client;
}

shared_ptr<InstanceDatabaseClientProviderInterface>
FakeInstanceDatabaseClientProviderFactory::CreateClient(
    const shared_ptr<AutoScalingClientOptions>& client_options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  auto client = make_shared<NiceMock<MockInstanceDatabaseClientProvider>>();
  ON_CALL(*client, GetInstanceByName)
      .WillByDefault([=](AsyncContext<GetInstanceByNameRequest,
                                      GetInstanceByNameResponse>& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetInstanceByNameResponse>();
        context.response->mutable_instance()->set_instance_name(kInstanceName);
        context.response->mutable_instance()->set_status(
            InstanceStatus::TERMINATING_WAIT);
        context.Finish();
        return SuccessExecutionResult();
      });
  return client;
}

shared_ptr<AutoScalingClientProviderInterface>
AutoScalingClientProviderFactory::Create(
    const shared_ptr<AutoScalingClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpAutoScalingClientProvider>(
      options, instance_client_provider, cpu_async_executor, io_async_executor,
      make_shared<FakeInstanceDatabaseClientProviderFactory>(),
      make_shared<FakeInstanceGroupManagersClientFactory>());
}
}  // namespace google::scp::cpio::client_providers
