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
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/nosql_database_client/nosql_database_client_interface.h"
#include "public/cpio/interface/nosql_database_client/type_def.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for queuing messages.
 */
class NoSQLDatabaseClient : public NoSQLDatabaseClientInterface {
 public:
  explicit NoSQLDatabaseClient(
      const std::shared_ptr<NoSQLDatabaseClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  void CreateTable(core::AsyncContext<
                   cmrt::sdk::nosql_database_service::v1::CreateTableRequest,
                   cmrt::sdk::nosql_database_service::v1::CreateTableResponse>&
                       create_table_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::CreateTableResponse>
  CreateTableSync(cmrt::sdk::nosql_database_service::v1::CreateTableRequest
                      request) noexcept override;

  void DeleteTable(core::AsyncContext<
                   cmrt::sdk::nosql_database_service::v1::DeleteTableRequest,
                   cmrt::sdk::nosql_database_service::v1::DeleteTableResponse>&
                       delete_table_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::DeleteTableResponse>
  DeleteTableSync(cmrt::sdk::nosql_database_service::v1::DeleteTableRequest
                      request) noexcept override;

  void GetDatabaseItem(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>
  GetDatabaseItemSync(
      cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest
          request) noexcept override;

  void CreateDatabaseItem(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemResponse>&
          create_database_item_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemResponse>
  CreateDatabaseItemSync(
      cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest
          request) noexcept override;

  void UpsertDatabaseItem(
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>
  UpsertDatabaseItemSync(
      cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest
          request) noexcept override;

 protected:
  std::shared_ptr<client_providers::NoSQLDatabaseClientProviderInterface>
      nosql_database_client_provider_;
  std::shared_ptr<NoSQLDatabaseClientOptions> options_;
};
}  // namespace google::scp::cpio
