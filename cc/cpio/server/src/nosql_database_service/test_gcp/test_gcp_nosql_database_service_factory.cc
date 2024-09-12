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

#include "test_gcp_nosql_database_service_factory.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/common/test/gcp/test_gcp_database_factory.h"
#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_nosql_database_client_provider.h"
#include "cpio/server/src/instance_service/test_gcp/test_gcp_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/proto/nosql_database_service/v1/test_configuration_keys.pb.h"
#include "public/cpio/test/nosql_database_client/test_gcp_nosql_database_client_options.h"

using google::cmrt::sdk::nosql_database_service::v1::
    TestClientConfigurationKeys;
using google::cmrt::sdk::nosql_database_service::v1::
    TestClientConfigurationKeys_Name;
using google::scp::cpio::client_providers::GcpNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using google::scp::cpio::client_providers::TestGcpDatabaseFactory;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::move;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
TestGcpNoSQLDatabaseServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<TestGcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

shared_ptr<InstanceServiceFactoryOptions> TestGcpNoSQLDatabaseServiceFactory::
    CreateInstanceServiceFactoryOptions() noexcept {
  auto options =
      GcpNoSQLDatabaseServiceFactory::CreateInstanceServiceFactoryOptions();
  auto test_options =
      make_shared<TestGcpInstanceServiceFactoryOptions>(*options);
  test_options->project_id_config_label = TestClientConfigurationKeys_Name(
      TestClientConfigurationKeys::CMRT_TEST_NOSQL_DATABASE_CLIENT_OWNER_ID);
  test_options->zone_config_label = TestClientConfigurationKeys_Name(
      TestClientConfigurationKeys::CMRT_TEST_NOSQL_DATABASE_CLIENT_ZONE);
  test_options->instance_id_config_label = TestClientConfigurationKeys_Name(
      TestClientConfigurationKeys::CMRT_TEST_NOSQL_DATABASE_CLIENT_INSTANCE_ID);
  return test_options;
}

shared_ptr<NoSQLDatabaseClientOptions> TestGcpNoSQLDatabaseServiceFactory::
    CreateNoSQLDatabaseClientOptions() noexcept {
  auto client_options =
      GcpNoSQLDatabaseServiceFactory::CreateNoSQLDatabaseClientOptions();
  auto test_options =
      make_shared<TestGcpNoSQLDatabaseClientOptions>(move(*client_options));

  TryReadConfigString(
      config_provider_,
      TestClientConfigurationKeys_Name(
          TestClientConfigurationKeys::
              CMRT_TEST_NOSQL_DATABASE_CLIENT_CLOUD_ENDPOINT_OVERRIDE),
      test_options->spanner_endpoint_override);
  return test_options;
}

shared_ptr<NoSQLDatabaseClientProviderInterface>
TestGcpNoSQLDatabaseServiceFactory::CreateNoSQLDatabaseClient() noexcept {
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
