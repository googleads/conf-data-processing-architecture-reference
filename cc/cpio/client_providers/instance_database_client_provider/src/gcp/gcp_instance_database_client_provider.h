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

#include <memory>
#include <string>
#include <utility>

#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/common/src/gcp/gcp_database_factory.h"
#include "cpio/client_providers/instance_database_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_database_client_provider_interface.h"
#include "google/cloud/spanner/client.h"

namespace google::scp::cpio::client_providers {
static constexpr char kInstanceNameColumnName[] = "InstanceName";
static constexpr char kInstanceStatusColumnName[] = "Status";
static constexpr char kRequestTimeColumnName[] = "RequestTime";
static constexpr char kTerminationTimeColumnName[] = "TerminationTime";
static constexpr char kTTLColumnName[] = "TTL";

/*! @copydoc InstanceDatabaseProviderInterface
 */
class GcpInstanceDatabaseClientProvider
    : public InstanceDatabaseClientProviderInterface {
 public:
  /**
   * @brief Construct a new Gcp Instance Database Client Provider object
   *
   * @param client_options Options for the Client.
   * @param instance_client Client used for getting the current instance
   * information.
   * @param cpu_async_executor Executor to run the async operations on.
   * @param io_async_executor Executor to run the IO async operations on.
   * @param gcp_database_factory Factory which provides the Instance Database
   * Client.
   */
  GcpInstanceDatabaseClientProvider(
      std::shared_ptr<InstanceDatabaseClientOptions> client_options,
      std::shared_ptr<InstanceClientProviderInterface> instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      std::shared_ptr<GcpDatabaseFactory> gcp_database_factory)
      : client_options_(client_options),
        instance_client_(instance_client),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        gcp_database_factory_(gcp_database_factory) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  void GetInstanceByName(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::GetInstanceByNameRequest,
          cmrt::sdk::instance_database_client::GetInstanceByNameResponse>&
          get_instance_context) noexcept override;

  void ListInstancesByStatus(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::ListInstancesByStatusRequest,
          cmrt::sdk::instance_database_client::ListInstancesByStatusResponse>&
          list_instances_context) noexcept override;

  void UpdateInstance(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::UpdateInstanceRequest,
          cmrt::sdk::instance_database_client::UpdateInstanceResponse>&
          update_instance_context) noexcept override;

 private:
  /**
   * @brief Is called by async executor in order to get instance by name.
   *
   * @param get_instance_context The context object of the get instance by name
   * operation.
   * @param query sql query to execute.
   */
  void GetInstanceByNameInternal(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::GetInstanceByNameRequest,
          cmrt::sdk::instance_database_client::GetInstanceByNameResponse>
          get_instance_context,
      std::string query) noexcept;
  /**
   * @brief Is called by async executor in order to list instances by status.
   *
   * @param list_instances_context The context object of the list instances by
   * status operation.
   * @param query sql query to execute.
   */
  void ListInstancesByStatusInternal(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::ListInstancesByStatusRequest,
          cmrt::sdk::instance_database_client::ListInstancesByStatusResponse>
          list_instances_context,
      std::string query) noexcept;
  /**
   * @brief Is called by async executor in order to update instance.
   *
   * @param create_table_context The context object of the update instance
   * operation.
   */
  void UpdateInstanceInternal(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::UpdateInstanceRequest,
          cmrt::sdk::instance_database_client::UpdateInstanceResponse>
          update_instance_context) noexcept;

  /// Options for the client.
  std::shared_ptr<InstanceDatabaseClientOptions> client_options_;

  /// Instance client.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_;

  /// An instance of the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;

  /// An instance of the GCP Spanner client. To enable thread safety of the
  /// encapsulating class, this instance is marked as const so it can't actually
  /// be used to query Spanner. Instead, Get/Upsert create a copy of this Client
  /// (which is a cheap operation) to query Spanner.
  std::shared_ptr<const google::cloud::spanner::Client> spanner_client_shared_;

  std::shared_ptr<GcpDatabaseFactory> gcp_database_factory_;
};

}  // namespace google::scp::cpio::client_providers
