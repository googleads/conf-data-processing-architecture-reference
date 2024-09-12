
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

#include "test_gcp_auto_scaling_client_provider.h"

#include <memory>
#include <string>

#include "cpio/client_providers/instance_database_client_provider/test/gcp/test_gcp_instance_database_client_provider.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/auto_scaling_client/type_def.h"
#include "public/cpio/test/auto_scaling_client/test_gcp_auto_scaling_client_options.h"

using google::cloud::MakeGoogleDefaultCredentials;
using google::cloud::MakeImpersonateServiceAccountCredentials;
using google::cloud::Options;
using google::scp::core::AsyncExecutorInterface;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
Options TestInstanceGroupManagersClientFactory::CreateClientOptions(
    const shared_ptr<AutoScalingClientOptions>& options) noexcept {
  Options client_options =
      InstanceGroupManagersClientFactory::CreateClientOptions(options);
  auto test_options =
      dynamic_pointer_cast<TestGcpAutoScalingClientOptions>(options);
  if (!test_options->impersonate_service_account.empty()) {
    client_options.set<google::cloud::UnifiedCredentialsOption>(
        (MakeImpersonateServiceAccountCredentials(
            google::cloud::MakeGoogleDefaultCredentials(),
            test_options->impersonate_service_account)));
  }
  return client_options;
}

shared_ptr<InstanceDatabaseClientProviderInterface>
TestInstanceDatabaseClientProviderFactory::CreateClient(
    const shared_ptr<AutoScalingClientOptions>& client_options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  auto test_options =
      dynamic_pointer_cast<TestGcpAutoScalingClientOptions>(client_options);
  auto instance_database_client_options =
      make_shared<TestGcpInstanceDatabaseClientOptions>();
  instance_database_client_options->gcp_spanner_instance_name =
      client_options->gcp_spanner_instance_name;
  instance_database_client_options->gcp_spanner_database_name =
      client_options->gcp_spanner_database_name;
  instance_database_client_options->instance_table_name =
      client_options->instance_table_name;
  instance_database_client_options->impersonate_service_account =
      test_options->impersonate_service_account;
  return make_shared<GcpInstanceDatabaseClientProvider>(
      instance_database_client_options, instance_client, cpu_async_executor,
      io_async_executor,
      make_shared<TestGcpDatabaseFactory>(
          make_shared<TestGcpDatabaseClientOptions>(
              instance_database_client_options
                  ->ToTestGcpDatabaseClientOptions())));
}

shared_ptr<AutoScalingClientProviderInterface>
AutoScalingClientProviderFactory::Create(
    const shared_ptr<AutoScalingClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpAutoScalingClientProvider>(
      options, instance_client_provider, cpu_async_executor, io_async_executor,
      make_shared<TestInstanceDatabaseClientProviderFactory>(),
      make_shared<TestInstanceGroupManagersClientFactory>());
}
}  // namespace google::scp::cpio::client_providers
