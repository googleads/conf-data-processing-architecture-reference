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

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_database_client_provider_interface.h"
#include "google/cloud/compute/region_instance_group_managers/v1/region_instance_group_managers_client.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class InstanceGroupManagersClientFactory;
class InstanceDatabaseClientProviderFactory;

/*! @copydoc AutoScalingClientInterface
 */
class GcpAutoScalingClientProvider : public AutoScalingClientProviderInterface {
 public:
  /**
   * @brief Constructs a new Gcp AutoScaling Client Provider object
   *
   * @param instance_client_provider Gcp instance client.
   * @param instance_group_managers_client_factory provides Gcp
   * AutoScalingClient.
   * @param io_async_executor The Gcp io async executor.
   */
  GcpAutoScalingClientProvider(
      const std::shared_ptr<AutoScalingClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<InstanceDatabaseClientProviderFactory>&
          instance_database_client_provider_factory =
              std::make_shared<InstanceDatabaseClientProviderFactory>(),
      const std::shared_ptr<InstanceGroupManagersClientFactory>&
          instance_group_managers_client_factory =
              std::make_shared<InstanceGroupManagersClientFactory>())
      : options_(options),
        instance_client_provider_(instance_client_provider),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        instance_database_client_provider_factory_(
            instance_database_client_provider_factory),
        instance_group_managers_client_factory_(
            instance_group_managers_client_factory) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  void TryFinishInstanceTermination(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>&
          context) noexcept override;

 private:
  void OnGetInstanceByNameCallback(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>&
          try_termination_context,
      core::AsyncContext<
          cmrt::sdk::instance_database_client::GetInstanceByNameRequest,
          cmrt::sdk::instance_database_client::GetInstanceByNameResponse>&
          get_instance_context) noexcept;

  void OnDeleteInstanceCallback(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>&
          try_termination_context,
      cloud::future<cloud::StatusOr<cloud::cpp::compute::v1::Operation>>
          delete_result) noexcept;

  std::shared_ptr<AutoScalingClientOptions> options_;
  std::string current_project_id_;
  std::string current_region_;
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;
  std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_;
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
  std::shared_ptr<InstanceDatabaseClientProviderFactory>
      instance_database_client_provider_factory_;
  std::shared_ptr<InstanceDatabaseClientProviderInterface>
      instance_database_client_provider_;
  std::shared_ptr<InstanceGroupManagersClientFactory>
      instance_group_managers_client_factory_;
  std::shared_ptr<cloud::compute_region_instance_group_managers_v1::
                      RegionInstanceGroupManagersClient>
      instance_group_managers_client_;
};

/// Provides RegionInstanceGroupManagersClient.
class InstanceGroupManagersClientFactory {
 public:
  virtual ~InstanceGroupManagersClientFactory() = default;

  /**
   * @brief Creates RegionInstanceGroupManagersClient.
   */
  virtual std::shared_ptr<cloud::compute_region_instance_group_managers_v1::
                              RegionInstanceGroupManagersClient>
  CreateClient(
      const std::shared_ptr<AutoScalingClientOptions>& options) noexcept;

  virtual cloud::Options CreateClientOptions(
      const std::shared_ptr<AutoScalingClientOptions>& options) noexcept;
};

/// Provides InstanceDatabaseClientProvider.
class InstanceDatabaseClientProviderFactory {
 public:
  virtual ~InstanceDatabaseClientProviderFactory() = default;

  /**
   * @brief Creates InstanceDatabaseClientProvider.
   */
  virtual std::shared_ptr<InstanceDatabaseClientProviderInterface> CreateClient(
      const std::shared_ptr<AutoScalingClientOptions>& client_options,
      const std::shared_ptr<InstanceClientProviderInterface>& instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers
