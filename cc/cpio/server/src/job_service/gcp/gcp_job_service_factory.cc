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

#include "gcp_job_service_factory.h"

#include <memory>

#include "cc/core/common/uuid/src/uuid.h"
#include "cpio/client_providers/job_client_provider/src/gcp/gcp_job_client_provider.h"
#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_nosql_database_client_provider.h"
#include "cpio/client_providers/queue_client_provider/src/gcp/gcp_queue_client_provider.h"
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/proto/job_service/v1/configuration_keys.pb.h"

using google::cmrt::sdk::job_service::v1::ClientConfigurationKeys;
using google::cmrt::sdk::job_service::v1::ClientConfigurationKeys_Name;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::GcpDatabaseFactory;
using google::scp::cpio::client_providers::GcpJobClientProvider;
using google::scp::cpio::client_providers::GcpNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::GcpQueueClientProvider;
using google::scp::cpio::client_providers::JobClientProviderInterface;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using google::scp::cpio::client_providers::QueueClientProviderInterface;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {

shared_ptr<JobClientOptions>
GcpJobServiceFactory::CreateJobClientOptions() noexcept {
  auto options = JobServiceFactory::CreateJobClientOptions();

  options->gcp_spanner_database_name = ReadConfigString(
      config_provider_,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::CMRT_GCP_JOB_CLIENT_SPANNER_DATABASE_NAME));

  options->gcp_spanner_instance_name = ReadConfigString(
      config_provider_,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::CMRT_GCP_JOB_CLIENT_SPANNER_INSTANCE_NAME));

  return options;
}

shared_ptr<InstanceServiceFactoryInterface>
GcpJobServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<GcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

shared_ptr<QueueClientProviderInterface>
GcpJobServiceFactory::CreateQueueClient() noexcept {
  return make_shared<GcpQueueClientProvider>(
      CreateQueueClientOptions(), instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

shared_ptr<NoSQLDatabaseClientOptions>
GcpJobServiceFactory::CreateNoSQLDatabaseClientOptions() noexcept {
  auto nosql_database_options =
      JobServiceFactory::CreateNoSQLDatabaseClientOptions();
  nosql_database_options->gcp_spanner_database_name =
      client_options_->gcp_spanner_database_name;
  nosql_database_options->gcp_spanner_instance_name =
      client_options_->gcp_spanner_instance_name;
  return nosql_database_options;
}

shared_ptr<NoSQLDatabaseClientProviderInterface>
GcpJobServiceFactory::CreateNoSQLDatabaseClient() noexcept {
  auto options = CreateNoSQLDatabaseClientOptions();
  return make_shared<GcpNoSQLDatabaseClientProvider>(
      options, instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor(),
      make_shared<GcpDatabaseFactory>(options));
}

shared_ptr<JobClientProviderInterface>
GcpJobServiceFactory::CreateJobClient() noexcept {
  return make_shared<GcpJobClientProvider>(
      client_options_, queue_client_, nosql_database_client_,
      instance_service_factory_->GetIoAsynceExecutor());
}
}  // namespace google::scp::cpio
