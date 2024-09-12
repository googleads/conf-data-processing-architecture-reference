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

#include "gcp_nosql_database_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "cc/core/common/uuid/src/uuid.h"
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_nosql_database_client_provider.h"
#include "cpio/server/interface/nosql_database_service/nosql_database_service_factory_interface.h"
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/proto/nosql_database_service/v1/configuration_keys.pb.h"

using google::cmrt::sdk::nosql_database_service::v1::ClientConfigurationKeys;
using google::cmrt::sdk::nosql_database_service::v1::
    ClientConfigurationKeys_Name;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::GcpDatabaseFactory;
using google::scp::cpio::client_providers::GcpNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace {
constexpr char kGcpNoSQLDatabaseServiceFactory[] =
    "GcpNoSQLDatabaseServiceFactory";
}  // namespace

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
GcpNoSQLDatabaseServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<GcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

shared_ptr<NoSQLDatabaseClientOptions>
GcpNoSQLDatabaseServiceFactory::CreateNoSQLDatabaseClientOptions() noexcept {
  auto client_options = make_shared<NoSQLDatabaseClientOptions>();
  client_options->gcp_spanner_instance_name = ReadConfigString(
      config_provider_,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_INSTANCE_NAME));

  client_options->gcp_spanner_database_name = ReadConfigString(
      config_provider_,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_DATABASE_NAME));

  return client_options;
}

std::shared_ptr<NoSQLDatabaseClientProviderInterface>
GcpNoSQLDatabaseServiceFactory::CreateNoSQLDatabaseClient() noexcept {
  auto client_options = CreateNoSQLDatabaseClientOptions();
  return make_shared<GcpNoSQLDatabaseClientProvider>(
      client_options, instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor(),
      make_shared<GcpDatabaseFactory>(client_options));
}
}  // namespace google::scp::cpio
