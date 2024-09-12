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

#include "fake_gcp_kms_client_provider.h"

#include <memory>
#include <string>

#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/client_providers/kms_client_provider/mock/gcp/mock_gcp_key_management_service_client.h"
#include "cpio/client_providers/kms_client_provider/src/gcp/gcp_kms_client_provider.h"
#include "google/cloud/kms/key_management_client.h"

using google::cloud::kms::KeyManagementServiceClient;
using google::cloud::kms::v1::DecryptResponse;
using google::scp::core::AsyncExecutorInterface;
using std::make_shared;
using std::shared_ptr;
using std::string;
using testing::Return;

namespace google::scp::cpio::client_providers {
std::shared_ptr<GcpKeyManagementServiceClientInterface>
FakeGcpKmsFactory::CreateGcpKeyManagementServiceClient(
    const string& wip_provider,
    const string& service_account_to_impersonate) noexcept {
  auto mock_gcp_key_management_service_client =
      make_shared<mock::MockGcpKeyManagementServiceClient>();
  DecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  ON_CALL(*mock_gcp_key_management_service_client, Decrypt)
      .WillByDefault(Return(decrypt_response));
  return mock_gcp_key_management_service_client;
}

shared_ptr<KeyManagementServiceClient>
GcpKmsFactory::CreateKeyManagementServiceClient(
    const string& wip_provider,
    const string& service_account_to_impersonate) noexcept {
  return nullptr;
}

shared_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    const shared_ptr<KmsClientOptions>& options,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor,
    const std::shared_ptr<core::AsyncExecutorInterface>&
        cpu_async_executor) noexcept {
  return make_shared<GcpKmsClientProvider>(
      io_async_executor, cpu_async_executor, make_shared<FakeGcpKmsFactory>());
}
}  // namespace google::scp::cpio::client_providers
