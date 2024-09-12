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

#include "fake_gcp_parameter_client_provider.h"

#include <memory>
#include <string>

#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "google/cloud/secretmanager/mocks/mock_secret_manager_connection.h"

using google::cloud::secretmanager::SecretManagerServiceClient;
using google::cloud::secretmanager::v1::AccessSecretVersionResponse;
using google::cloud::secretmanager_mocks::MockSecretManagerServiceConnection;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;
using testing::NiceMock;
using testing::Return;

namespace google::scp::cpio::client_providers {
shared_ptr<SecretManagerServiceClient> FakeSecretManagerFactory::CreateClient(
    const shared_ptr<ParameterClientOptions>& options) noexcept {
  auto connection = make_shared<NiceMock<MockSecretManagerServiceConnection>>();

  AccessSecretVersionResponse response;
  response.mutable_payload()->set_data(kParameterValue);
  ON_CALL(*connection, AccessSecretVersion).WillByDefault(Return(response));
  return make_shared<SecretManagerServiceClient>(connection);
}

shared_ptr<ParameterClientProviderInterface>
ParameterClientProviderFactory::Create(
    const shared_ptr<ParameterClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpParameterClientProvider>(
      cpu_async_executor, io_async_executor, instance_client_provider, options,
      make_shared<FakeSecretManagerFactory>());
}
}  // namespace google::scp::cpio::client_providers
