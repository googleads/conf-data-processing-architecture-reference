
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

#include "test_gcp_blob_storage_client_provider.h"

#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/type_def.h"
#include "public/cpio/test/blob_storage_client/test_gcp_blob_storage_client_options.h"

using google::cloud::MakeGoogleDefaultCredentials;
using google::cloud::MakeImpersonateServiceAccountCredentials;
using google::cloud::Options;
using google::scp::core::AsyncExecutorInterface;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
Options TestGcpCloudStorageFactory::CreateClientOptions(
    shared_ptr<BlobStorageClientOptions> options, const string& project_id,
    const string& wip_provider) noexcept {
  Options client_options = GcpCloudStorageFactory::CreateClientOptions(
      options, project_id, wip_provider);
  auto test_options =
      dynamic_pointer_cast<TestGcpBlobStorageClientOptions>(options);
  if (!test_options->impersonate_service_account.empty()) {
    client_options.set<google::cloud::UnifiedCredentialsOption>(
        (MakeImpersonateServiceAccountCredentials(
            google::cloud::MakeGoogleDefaultCredentials(),
            test_options->impersonate_service_account)));
  }
  if (!test_options->gcs_endpoint_override.empty()) {
    client_options.set<google::cloud::UnifiedCredentialsOption>(
        google::cloud::MakeInsecureCredentials());
    client_options.set<google::cloud::EndpointOption>(
        test_options->gcs_endpoint_override);
  }
  return client_options;
}

shared_ptr<BlobStorageClientProviderInterface>
BlobStorageClientProviderFactory::Create(
    shared_ptr<BlobStorageClientOptions> options,
    shared_ptr<InstanceClientProviderInterface> instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  return make_shared<GcpBlobStorageClientProvider>(
      options, instance_client_provider, cpu_async_executor, io_async_executor,
      make_shared<TestGcpCloudStorageFactory>());
}
}  // namespace google::scp::cpio::client_providers
