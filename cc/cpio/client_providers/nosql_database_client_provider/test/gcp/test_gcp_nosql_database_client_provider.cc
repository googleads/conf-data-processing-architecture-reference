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

#include "cpio/client_providers/common/test/gcp/test_gcp_database_factory.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_nosql_database_client_provider.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/type_def.h"
#include "public/cpio/test/nosql_database_client/test_gcp_nosql_database_client_options.h"

using google::cloud::MakeGoogleDefaultCredentials;
using google::cloud::MakeImpersonateServiceAccountCredentials;
using google::cloud::Options;
using google::scp::core::AsyncExecutorInterface;
using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
shared_ptr<NoSQLDatabaseClientProviderInterface>
NoSQLDatabaseClientProviderFactory::Create(
    const shared_ptr<NoSQLDatabaseClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  auto test_options =
      dynamic_pointer_cast<TestGcpNoSQLDatabaseClientOptions>(options);
  return make_shared<GcpNoSQLDatabaseClientProvider>(
      options, instance_client, cpu_async_executor, io_async_executor,
      std::make_shared<TestGcpDatabaseFactory>(
          std::make_shared<TestGcpDatabaseClientOptions>(
              test_options->ToTestGcpDatabaseClientOptions())));
}
}  // namespace google::scp::cpio::client_providers
