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

#include "nosql_database_client.h"

#include <functional>
#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

using google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::CreateTableRequest;
using google::cmrt::sdk::nosql_database_service::v1::CreateTableResponse;
using google::cmrt::sdk::nosql_database_service::v1::DeleteTableRequest;
using google::cmrt::sdk::nosql_database_service::v1::DeleteTableResponse;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderFactory;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using std::placeholders::_1;

namespace {
constexpr char kNoSQLDatabaseClient[] = "NoSQLDatabaseClient";
}  // namespace

namespace google::scp::cpio {
ExecutionResult NoSQLDatabaseClient::Init() noexcept {
  shared_ptr<InstanceClientProviderInterface> instance_client;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(instance_client),
      kNoSQLDatabaseClient, kZeroUuid, "Failed to get InstanceClientProvider.");

  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(cpu_async_executor),
      kNoSQLDatabaseClient, kZeroUuid, "Failed to get CpuAsyncExecutor.");

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor),
      kNoSQLDatabaseClient, kZeroUuid, "Failed to get IoAsyncExecutor.");

  nosql_database_client_provider_ = NoSQLDatabaseClientProviderFactory::Create(
      options_, instance_client, cpu_async_executor, io_async_executor);
  RETURN_AND_LOG_IF_FAILURE(
      nosql_database_client_provider_->Init(), kNoSQLDatabaseClient, kZeroUuid,
      "Failed to initialize NoSQLDatabaseClientProvider.");

  return SuccessExecutionResult();
}

ExecutionResult NoSQLDatabaseClient::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(nosql_database_client_provider_->Run(),
                            kNoSQLDatabaseClient, kZeroUuid,
                            "Failed to run NoSQLDatabaseClientProvider.");
  return SuccessExecutionResult();
}

ExecutionResult NoSQLDatabaseClient::Stop() noexcept {
  RETURN_AND_LOG_IF_FAILURE(nosql_database_client_provider_->Stop(),
                            kNoSQLDatabaseClient, kZeroUuid,
                            "Failed to stop NoSQLDatabaseClientProvider.");
  return SuccessExecutionResult();
}

void NoSQLDatabaseClient::CreateTable(
    AsyncContext<CreateTableRequest, CreateTableResponse>&
        create_table_context) noexcept {
  nosql_database_client_provider_->CreateTable(create_table_context);
}

ExecutionResultOr<CreateTableResponse> NoSQLDatabaseClient::CreateTableSync(
    CreateTableRequest request) noexcept {
  CreateTableResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<CreateTableRequest, CreateTableResponse>(
          bind(&NoSQLDatabaseClient::CreateTable, this, _1), move(request),
          response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kNoSQLDatabaseClient, kZeroUuid,
                            "Failed to create table.");
  return response;
}

void NoSQLDatabaseClient::DeleteTable(
    AsyncContext<DeleteTableRequest, DeleteTableResponse>&
        delete_table_context) noexcept {
  nosql_database_client_provider_->DeleteTable(delete_table_context);
}

ExecutionResultOr<DeleteTableResponse> NoSQLDatabaseClient::DeleteTableSync(
    DeleteTableRequest request) noexcept {
  DeleteTableResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<DeleteTableRequest, DeleteTableResponse>(
          bind(&NoSQLDatabaseClient::DeleteTable, this, _1), move(request),
          response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kNoSQLDatabaseClient, kZeroUuid,
                            "Failed to delete table.");
  return response;
}

void NoSQLDatabaseClient::GetDatabaseItem(
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  nosql_database_client_provider_->GetDatabaseItem(get_database_item_context);
}

ExecutionResultOr<GetDatabaseItemResponse>
NoSQLDatabaseClient::GetDatabaseItemSync(
    GetDatabaseItemRequest request) noexcept {
  GetDatabaseItemResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetDatabaseItemRequest, GetDatabaseItemResponse>(
          bind(&NoSQLDatabaseClient::GetDatabaseItem, this, _1), move(request),
          response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kNoSQLDatabaseClient, kZeroUuid,
                            "Failed to get database item.");
  return response;
}

void NoSQLDatabaseClient::CreateDatabaseItem(
    AsyncContext<CreateDatabaseItemRequest, CreateDatabaseItemResponse>&
        create_database_item_context) noexcept {
  nosql_database_client_provider_->CreateDatabaseItem(
      create_database_item_context);
}

ExecutionResultOr<CreateDatabaseItemResponse>
NoSQLDatabaseClient::CreateDatabaseItemSync(
    CreateDatabaseItemRequest request) noexcept {
  CreateDatabaseItemResponse response;
  auto execution_result = SyncUtils::AsyncToSync2<CreateDatabaseItemRequest,
                                                  CreateDatabaseItemResponse>(
      bind(&NoSQLDatabaseClient::CreateDatabaseItem, this, _1), move(request),
      response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kNoSQLDatabaseClient, kZeroUuid,
                            "Failed to create database item.");
  return response;
}

void NoSQLDatabaseClient::UpsertDatabaseItem(
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context) noexcept {
  return nosql_database_client_provider_->UpsertDatabaseItem(
      upsert_database_item_context);
}

ExecutionResultOr<UpsertDatabaseItemResponse>
NoSQLDatabaseClient::UpsertDatabaseItemSync(
    UpsertDatabaseItemRequest request) noexcept {
  UpsertDatabaseItemResponse response;
  auto execution_result = SyncUtils::AsyncToSync2<UpsertDatabaseItemRequest,
                                                  UpsertDatabaseItemResponse>(
      bind(&NoSQLDatabaseClient::UpsertDatabaseItem, this, _1), move(request),
      response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kNoSQLDatabaseClient, kZeroUuid,
                            "Failed to upsert database item.");
  return response;
}

unique_ptr<NoSQLDatabaseClientInterface> NoSQLDatabaseClientFactory::Create(
    NoSQLDatabaseClientOptions options) noexcept {
  return make_unique<NoSQLDatabaseClient>(
      make_shared<NoSQLDatabaseClientOptions>(options));
}
}  // namespace google::scp::cpio
