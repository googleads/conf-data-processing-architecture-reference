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

#include "test_gcp_queue_service_factory.h"

#include <memory>

#include "cpio/client_providers/queue_client_provider/test/gcp/test_gcp_queue_client_provider.h"
#include "cpio/server/src/instance_service/test_gcp/test_gcp_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/proto/queue_service/v1/test_configuration_keys.pb.h"

using google::cmrt::sdk::queue_service::v1::TestClientConfigurationKeys;
using google::cmrt::sdk::queue_service::v1::TestClientConfigurationKeys_Name;
using google::scp::cpio::client_providers::QueueClientProviderInterface;
using google::scp::cpio::client_providers::TestGcpQueueClientProvider;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<QueueClientOptions>
TestGcpQueueServiceFactory::CreateQueueClientOptions() noexcept {
  auto test_options = make_shared<TestGcpQueueClientOptions>(
      *QueueServiceFactory::CreateQueueClientOptions());

  TryReadConfigString(config_provider_,
                      TestClientConfigurationKeys_Name(
                          TestClientConfigurationKeys::
                              CMRT_TEST_QUEUE_CLIENT_CLOUD_ENDPOINT_OVERRIDE),
                      test_options->pubsub_endpoint_override);

  return test_options;
}

shared_ptr<QueueClientProviderInterface>
TestGcpQueueServiceFactory::CreateQueueClient() noexcept {
  return make_shared<TestGcpQueueClientProvider>(
      dynamic_pointer_cast<TestGcpQueueClientOptions>(queue_client_options_),
      instance_client_, instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

shared_ptr<InstanceServiceFactoryInterface>
TestGcpQueueServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<TestGcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

shared_ptr<InstanceServiceFactoryOptions>
TestGcpQueueServiceFactory::CreateInstanceServiceFactoryOptions() noexcept {
  auto options = GcpQueueServiceFactory::CreateInstanceServiceFactoryOptions();
  auto test_options =
      make_shared<TestGcpInstanceServiceFactoryOptions>(*options);
  test_options->project_id_config_label = TestClientConfigurationKeys_Name(
      TestClientConfigurationKeys::CMRT_TEST_QUEUE_CLIENT_OWNER_ID);
  test_options->zone_config_label = TestClientConfigurationKeys_Name(
      TestClientConfigurationKeys::CMRT_TEST_QUEUE_CLIENT_ZONE);
  test_options->instance_id_config_label = TestClientConfigurationKeys_Name(
      TestClientConfigurationKeys::CMRT_TEST_QUEUE_CLIENT_INSTANCE_ID);
  return test_options;
}
}  // namespace google::scp::cpio
