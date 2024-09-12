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

#ifndef SCP_CPIO_INTERFACE_NOSQL_DATABASE_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_NOSQL_DATABASE_CLIENT_INTERFACE_H_

#include <memory>
#include <string>

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {

/**
 * @brief Interface for interacting with NoSQLDatabase providers.
 */
class NoSQLDatabaseClientInterface : public core::ServiceInterface {
 public:
  virtual ~NoSQLDatabaseClientInterface() = default;

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
   * @brief Creates a table in a blocking call.
   *
   * @param request The request for the operation.
   * @return ExecutionResultOr<CreateTableResponse> The execution result of the
   * operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::CreateTableResponse>
  CreateTableSync(cmrt::sdk::nosql_database_service::v1::CreateTableRequest
                      request) noexcept = 0;

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
   * @brief Deletes a table in a blocking call.
   *
   * @param request The request for the operation.
   * @return ExecutionResultOr<DeleteTableResponse> The execution result of the
   * operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::DeleteTableResponse>
  DeleteTableSync(cmrt::sdk::nosql_database_service::v1::DeleteTableRequest
                      request) noexcept = 0;

  /**
   * @brief Gets a database record using provided metadata.
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
   * @brief Gets a database record using provided metadata in a blocking call.
   *
   * @param request The request for the operation.
   * @return ExecutionResultOr<GetDatabaseItemResponse> The execution result of
   * the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>
  GetDatabaseItemSync(
      cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest
          request) noexcept = 0;

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
   * @brief Creates a database record using provided metadata in a blocking
   * call.
   *
   * @param request The request for the operation.
   * @return ExecutionResultOr<CreateDatabaseItemResponse> The execution result
   * of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemResponse>
  CreateDatabaseItemSync(
      cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest
          request) noexcept = 0;

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

  /**
   * @brief Upserts a database record using provided metadata in a blocking
   * call.
   *
   * @param request The request for the operation.
   * @return ExecutionResultOr<UpsertDatabaseItemResponse> The execution result
   * of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>
  UpsertDatabaseItemSync(
      cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest
          request) noexcept = 0;
};

class NoSQLDatabaseClientFactory {
 public:
  /**
   * @brief Factory to create NoSQLDatabaseClientInterface.
   *
   * @param options NoSQLDatabaseClientOptions.
   * @return std::shared_ptr<NoSQLDatabaseClientProviderInterface>
   */
  static std::unique_ptr<NoSQLDatabaseClientInterface> Create(
      NoSQLDatabaseClientOptions options =
          NoSQLDatabaseClientOptions()) noexcept;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_NOSQL_DATABASE_CLIENT_INTERFACE_H_
