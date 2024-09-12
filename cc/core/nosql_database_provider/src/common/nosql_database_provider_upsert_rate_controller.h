/*
 * Copyright 2023 Google LLC
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

#include <memory>

#include "core/interface/async_context.h"
#include "core/interface/nosql_database_provider_interface.h"

#include "error_codes.h"

namespace google::scp::core {
class NoSQLDatabaseProviderUpsertRateController
    : public NoSQLDatabaseProviderInterface {
 public:
  // Rate control using a simple count threshold.
  NoSQLDatabaseProviderUpsertRateController(
      std::shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider,
      size_t max_outstanding_upsert_operations_count)
      : nosql_database_provider_(nosql_database_provider),
        current_outstanding_upsert_operations_count_(0),
        max_outstanding_upsert_operations_count_(
            max_outstanding_upsert_operations_count) {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult GetDatabaseItem(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
          get_database_item_context) noexcept override {
    return nosql_database_provider_->GetDatabaseItem(get_database_item_context);
  }

  ExecutionResult UpsertDatabaseItem(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept override {
    if (current_outstanding_upsert_operations_count_ >=
        max_outstanding_upsert_operations_count_) {
      return RetryExecutionResult(
          errors::SC_NO_SQL_DATABASE_PROVIDER_UPSERT_OPERATION_THROTTLED);
    }
    current_outstanding_upsert_operations_count_++;

    upsert_database_item_context.callback = std::bind(
        &NoSQLDatabaseProviderUpsertRateController::UpsertDatabaseItemCallback,
        this, upsert_database_item_context.callback, std::placeholders::_1);
    return nosql_database_provider_->UpsertDatabaseItem(
        upsert_database_item_context);
  }

 private:
  void UpsertDatabaseItemCallback(
      AsyncContext<UpsertDatabaseItemRequest,
                   UpsertDatabaseItemResponse>::Callback
          upsert_database_item_context_original_callback,
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context) {
    upsert_database_item_context_original_callback(
        upsert_database_item_context);
    current_outstanding_upsert_operations_count_--;
  }

  std::shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider_;
  std::atomic<size_t> current_outstanding_upsert_operations_count_;
  const size_t max_outstanding_upsert_operations_count_;
};
};  // namespace google::scp::core
