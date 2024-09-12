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
#include <unordered_map>
#include <utility>

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/cpio/interface/nosql_database_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

namespace google::scp::cpio::client_providers {

/**
 * @brief NoSQLDatabase provides database access APIs for single records.
 */
class NoSQLDatabaseClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~NoSQLDatabaseClientProviderInterface() = default;

  /**
   * @brief Creates a table.
   *
   * @param create_table_context The context object for the database
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual void CreateTable(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::CreateTableRequest,
          cmrt::sdk::nosql_database_service::v1::CreateTableResponse>&
          create_table_context) noexcept = 0;

  /**
   * @brief Deletes a table.
   *
   * @param delete_table_context The context object for the database
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual void DeleteTable(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::DeleteTableRequest,
          cmrt::sdk::nosql_database_service::v1::DeleteTableResponse>&
          delete_table_context) noexcept = 0;

  /**
   * @brief Gets a database record using provided metadta.
   *
   * @param get_database_item_context The context object for the database
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual void GetDatabaseItem(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context) noexcept = 0;

  /**
   * @brief Creates a database record using provided metadata.
   *
   * @param create_database_item_context The context object for the database
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual void CreateDatabaseItem(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemResponse>&
          create_database_item_context) noexcept = 0;

  /**
   * @brief Upserts a database record using provided metadata.
   *
   * @param upsert_database_item_context The context object for the database
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual void UpsertDatabaseItem(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept = 0;
};

class NoSQLDatabaseClientProviderFactory {
 public:
  /**
   * @brief Factory to create NoSQLDatabaseClientProviderInterface.
   *
   * @param options NoSQLDatabaseClientOptions.
   * @param instance_client Instance Client.
   * @param cpu_async_executor CPU Async Eexcutor.
   * @param io_async_executor IO Async Eexcutor.
   * @return std::shared_ptr<NoSQLDatabaseClientProviderInterface>
   */
  static std::shared_ptr<NoSQLDatabaseClientProviderInterface> Create(
      const std::shared_ptr<NoSQLDatabaseClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>& instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor);
};

}  // namespace google::scp::cpio::client_providers
