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

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "cpio/proto/instance_database_client.pb.h"
#include "public/cpio/interface/type_def.h"

#include "type_def.h"

namespace google::scp::cpio::client_providers {

/// Configurations for InstanceDatabaseClientProvider.
struct InstanceDatabaseClientOptions : public DatabaseClientOptions {
  InstanceDatabaseClientOptions() = default;

  virtual ~InstanceDatabaseClientOptions() = default;

  InstanceDatabaseClientOptions(std::string spanner_instance_name,
                                std::string spanner_database_name,
                                std::string input_instance_table_name)
      : DatabaseClientOptions(std::move(spanner_instance_name),
                              std::move(spanner_database_name)),
        instance_table_name(std::move(input_instance_table_name)) {}

  // The name for the instance table.
  std::string instance_table_name;
};

/// Provides Instance Database related functionality.
class InstanceDatabaseClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~InstanceDatabaseClientProviderInterface() = default;

  /**
   * @brief Gets the instance by the given instance name.
   *
   * @param get_instance_context the context of the get instance by name
   * operation.
   */
  virtual void GetInstanceByName(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::GetInstanceByNameRequest,
          cmrt::sdk::instance_database_client::GetInstanceByNameResponse>&
          get_instance_context) noexcept = 0;

  /**
   * @brief Lists the instances by the given instance status.
   *
   * @param list_instances_context the context of the list instances by status
   * operation.
   */
  virtual void ListInstancesByStatus(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::ListInstancesByStatusRequest,
          cmrt::sdk::instance_database_client::ListInstancesByStatusResponse>&
          list_instances_context) noexcept = 0;

  /**
   * @brief Updates the instance.
   *
   * @param update_instance_context the context of the update instance
   * operation.
   */
  virtual void UpdateInstance(
      core::AsyncContext<
          cmrt::sdk::instance_database_client::UpdateInstanceRequest,
          cmrt::sdk::instance_database_client::UpdateInstanceResponse>&
          update_instance_context) noexcept = 0;
};
}  // namespace google::scp::cpio::client_providers
