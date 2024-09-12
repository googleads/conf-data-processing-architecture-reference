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

#include "test_gcp_job_service_factory.h"

#include <memory>

#include "cpio/client_providers/common/test/gcp/test_gcp_database_factory.h"
#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_nosql_database_client_provider.h"
#include "cpio/client_providers/queue_client_provider/test/gcp/test_gcp_queue_client_provider.h"
#include "cpio/server/src/instance_service/test_gcp/test_gcp_instance_service_factory.h"
#include "cpio/server/src/job_service/gcp/gcp_job_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/proto/job_service/v1/test_configuration_keys.pb.h"
#include "public/cpio/test/job_client/test_gcp_job_client_options.h"
#include "public/cpio/test/nosql_database_client/test_gcp_nosql_database_client_options.h"

using google::cmrt::sdk::job_service::v1::TestClientConfigurationKeys;
using google::cmrt::sdk::job_service::v1::TestClientConfigurationKeys_Name;
using google::scp::cpio::client_providers::GcpNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::GcpQueueClientProvider;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using google::scp::cpio::client_providers::QueueClientProviderInterface;
using google::scp::cpio::client_providers::TestGcpDatabaseFactory;
using google::scp::cpio::client_providers::TestGcpQueueClientProvider;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
TestGcpJobServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<TestGcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

shared_ptr<InstanceServiceFactoryOptions>
TestGcpJobServiceFactory::CreateInstanceServiceFactoryOptions() noexcept {
  auto options = GcpJobServiceFactory::CreateInstanceServiceFactoryOptions();
  auto test_options =
      make_shared<TestGcpInstanceServiceFactoryOptions>(*options);
  test_options->project_id_config_label = TestClientConfigurationKeys_Name(
      TestClientConfigurationKeys::CMRT_TEST_JOB_CLIENT_OWNER_ID);
  return test_options;
}

shared_ptr<JobClientOptions>
TestGcpJobServiceFactory::CreateJobClientOptions() noexcept {
  auto test_options = make_shared<TestGcpJobClientOptions>(
      *GcpJobServiceFactory::CreateJobClientOptions());
  TryReadConfigString(
      config_provider_,
      TestClientConfigurationKeys_Name(
          TestClientConfigurationKeys::
              CMRT_TEST_GCP_JOB_CLIENT_IMPERSONATE_SERVICE_ACCOUNT),
      test_options->impersonate_service_account);
  TryReadConfigString(
      config_provider_,
      TestClientConfigurationKeys_Name(
          TestClientConfigurationKeys::CMRT_TEST_GCP_JOB_CLIENT_ACCESS_TOKEN),
      test_options->access_token);
  return test_options;
}

shared_ptr<QueueClientOptions>
TestGcpJobServiceFactory::CreateQueueClientOptions() noexcept {
  auto test_options = make_shared<TestGcpQueueClientOptions>(
      *JobServiceFactory::CreateQueueClientOptions());
  test_options->access_token =
      dynamic_pointer_cast<TestGcpJobClientOptions>(client_options_)
          ->access_token;
  return test_options;
}

shared_ptr<QueueClientProviderInterface>
TestGcpJobServiceFactory::CreateQueueClient() noexcept {
  return make_shared<TestGcpQueueClientProvider>(
      dynamic_pointer_cast<TestGcpQueueClientOptions>(
          CreateQueueClientOptions()),
      instance_client_, instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

shared_ptr<NoSQLDatabaseClientOptions>
TestGcpJobServiceFactory::CreateNoSQLDatabaseClientOptions() noexcept {
  auto test_options = make_shared<TestGcpNoSQLDatabaseClientOptions>(
      *GcpJobServiceFactory::CreateNoSQLDatabaseClientOptions());
  test_options->impersonate_service_account =
      dynamic_pointer_cast<TestGcpJobClientOptions>(client_options_)
          ->impersonate_service_account;
  return test_options;
}

shared_ptr<NoSQLDatabaseClientProviderInterface>
TestGcpJobServiceFactory::CreateNoSQLDatabaseClient() noexcept {
  auto options = CreateNoSQLDatabaseClientOptions();
  auto test_options =
      dynamic_pointer_cast<TestGcpNoSQLDatabaseClientOptions>(options);
  return make_shared<GcpNoSQLDatabaseClientProvider>(
      options, instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor(),
      std::make_shared<TestGcpDatabaseFactory>(
          std::make_shared<TestGcpDatabaseClientOptions>(
              test_options->ToTestGcpDatabaseClientOptions())));
}

}  // namespace google::scp::cpio
