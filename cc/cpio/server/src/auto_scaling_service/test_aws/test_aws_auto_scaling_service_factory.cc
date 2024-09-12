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

#include "test_aws_auto_scaling_service_factory.h"

#include <memory>

#include "cpio/client_providers/auto_scaling_client_provider/test/aws/test_aws_auto_scaling_client_provider.h"
#include "cpio/server/src/auto_scaling_service/aws/aws_auto_scaling_service_factory.h"
#include "cpio/server/src/instance_service/test_aws/test_aws_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/proto/auto_scaling_service/v1/test_configuration_keys.pb.h"

using google::cmrt::sdk::auto_scaling_service::v1::TestClientConfigurationKeys;
using google::cmrt::sdk::auto_scaling_service::v1::
    TestClientConfigurationKeys_Name;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::AutoScalingClientProviderInterface;
using google::scp::cpio::client_providers::AwsAutoScalingClientProvider;
using google::scp::cpio::client_providers::TestAwsAutoScalingClientOptions;
using google::scp::cpio::client_providers::TestAwsAutoScalingClientProvider;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
TestAwsAutoScalingServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<TestAwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

shared_ptr<InstanceServiceFactoryOptions> TestAwsAutoScalingServiceFactory::
    CreateInstanceServiceFactoryOptions() noexcept {
  auto options = make_shared<TestAwsInstanceServiceFactoryOptions>(
      *AwsAutoScalingServiceFactory::CreateInstanceServiceFactoryOptions());
  options->region_config_label = TestClientConfigurationKeys_Name(
      TestClientConfigurationKeys::CMRT_TEST_AUTO_SCALING_CLIENT_REGION);
  return options;
}

std::shared_ptr<AutoScalingClientProviderInterface>
TestAwsAutoScalingServiceFactory::CreateAutoScalingClient() noexcept {
  auto execution_result = TryReadConfigString(
      config_provider_,
      TestClientConfigurationKeys_Name(
          TestClientConfigurationKeys::
              CMRT_TEST_AUTO_SCALING_CLIENT_CLOUD_ENDPOINT_OVERRIDE),
      test_options_->auto_scaling_client_endpoint_override);
  if (execution_result.Successful() &&
      !test_options_->auto_scaling_client_endpoint_override.empty()) {
    return make_shared<TestAwsAutoScalingClientProvider>(
        test_options_, instance_client_,
        instance_service_factory_->GetIoAsynceExecutor());
  }
  return make_shared<AwsAutoScalingClientProvider>(
      test_options_, instance_client_,
      instance_service_factory_->GetIoAsynceExecutor());
}
}  // namespace google::scp::cpio
