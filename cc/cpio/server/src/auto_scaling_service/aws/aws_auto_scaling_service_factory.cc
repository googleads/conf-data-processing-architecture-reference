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

#include "aws_auto_scaling_service_factory.h"

#include <memory>
#include <utility>

#include "cc/core/common/uuid/src/uuid.h"
#include "cpio/client_providers/auto_scaling_client_provider/src/aws/aws_auto_scaling_client_provider.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/server/interface/auto_scaling_service/auto_scaling_service_factory_interface.h"
#include "cpio/server/src/instance_service/aws/aws_instance_service_factory.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::AutoScalingClientProviderInterface;
using google::scp::cpio::client_providers::AwsAutoScalingClientProvider;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
AwsAutoScalingServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<AwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<AutoScalingClientProviderInterface>
AwsAutoScalingServiceFactory::CreateAutoScalingClient() noexcept {
  return make_shared<AwsAutoScalingClientProvider>(
      CreateAutoScalingClientOptions(), instance_client_,
      instance_service_factory_->GetIoAsynceExecutor());
}
}  // namespace google::scp::cpio
