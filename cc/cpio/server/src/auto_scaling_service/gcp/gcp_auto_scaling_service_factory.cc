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

#include "gcp_auto_scaling_service_factory.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/auto_scaling_client_provider/src/gcp/gcp_auto_scaling_client_provider.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/server/interface/auto_scaling_service/auto_scaling_service_factory_interface.h"
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/proto/auto_scaling_service/v1/configuration_keys.pb.h"

using google::cmrt::sdk::auto_scaling_service::v1::ClientConfigurationKeys;
using google::cmrt::sdk::auto_scaling_service::v1::
    ClientConfigurationKeys_Name;
using google::scp::core::ExecutionResult;
using google::scp::cpio::client_providers::AutoScalingClientProviderInterface;
using google::scp::cpio::client_providers::GcpAutoScalingClientProvider;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {

shared_ptr<InstanceServiceFactoryInterface>
GcpAutoScalingServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<GcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

shared_ptr<AutoScalingClientProviderInterface>
GcpAutoScalingServiceFactory::CreateAutoScalingClient() noexcept {
  return make_shared<GcpAutoScalingClientProvider>(
      CreateAutoScalingClientOptions(), instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

shared_ptr<AutoScalingClientOptions>
GcpAutoScalingServiceFactory::CreateAutoScalingClientOptions() noexcept {
  auto options = make_shared<AutoScalingClientOptions>();
  options->instance_table_name = ReadConfigString(
      config_provider_, ClientConfigurationKeys_Name(
                            ClientConfigurationKeys::
                                CMRT_AUTO_SCALING_CLIENT_INSTANCE_TABLE_NAME));
  options->gcp_spanner_instance_name = ReadConfigString(
      config_provider_,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_INSTANCE_NAME));
  options->gcp_spanner_database_name = ReadConfigString(
      config_provider_,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_DATABASE_NAME));
  return options;
}
}  // namespace google::scp::cpio
