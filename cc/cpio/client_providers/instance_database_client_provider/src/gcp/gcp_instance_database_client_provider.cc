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

#include "gcp_instance_database_client_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/common/src/gcp/gcp_database_factory.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"
#include "cpio/client_providers/instance_database_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "cpio/proto/instance_database_client.pb.h"
#include "google/cloud/options.h"
#include "operator/protos/shared/backend/asginstance/instance_status.pb.h"

#include "gcp_instance_database_client_utils.h"

using google::cloud::Options;
using google::cloud::StatusOr;
using google::cloud::spanner::Client;
using google::cloud::spanner::Database;
using google::cloud::spanner::MakeConnection;
using google::cloud::spanner::MakeTimestamp;
using google::cloud::spanner::MakeUpdateMutation;
using google::cloud::spanner::Mutation;
using google::cloud::spanner::Mutations;
using google::cloud::spanner::RowStream;
using google::cloud::spanner::SqlStatement;
using google::cloud::spanner::Transaction;
using google::cloud::spanner::Value;
using google::cmrt::sdk::instance_database_client::GetInstanceByNameRequest;
using google::cmrt::sdk::instance_database_client::GetInstanceByNameResponse;
using google::cmrt::sdk::instance_database_client::ListInstancesByStatusRequest;
using google::cmrt::sdk::instance_database_client::
    ListInstancesByStatusResponse;
using google::cmrt::sdk::instance_database_client::UpdateInstanceRequest;
using google::cmrt::sdk::instance_database_client::UpdateInstanceResponse;
using google::protobuf::RepeatedPtrField;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FinishContext;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_GCP_FAILED_PRECONDITION;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND;
using google::scp::core::errors::
    SCP_INSTANCE_DATABASE_CLIENT_PROVIDER_COMMIT_FAILED;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::client_providers::GcpInstanceDatabaseClientUtils;
using google::scp::cpio::common::GcpUtils;
using google::scp::operators::protos::shared::backend::asginstance::
    InstanceStatus_Name;
using std::bind;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::move;
using std::optional;
using std::pair;
using std::ref;
using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;
using std::placeholders::_1;

namespace {
constexpr char kGcpInstanceDatabaseClientProvider[] =
    "GcpInstanceDatabaseClientProvider";
}  // namespace

namespace google::scp::cpio::client_providers {
static string ConstructInstanceQuery(string table_name, string filter_key,
                                     string filter_value) {
  return absl::StrFormat("SELECT %s, %s, %s, %s, %s FROM `%s` WHERE %s = '%s'",
                         kInstanceNameColumnName, kInstanceStatusColumnName,
                         kRequestTimeColumnName, kTerminationTimeColumnName,
                         kTTLColumnName, table_name, filter_key, filter_value);
}

ExecutionResult GcpInstanceDatabaseClientProvider::Init() noexcept {
  if (cpu_async_executor_ == nullptr) {
    auto result = FailureExecutionResult(
        SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INITIALIZATION_FAILED);
    SCP_ERROR(kGcpInstanceDatabaseClientProvider, kZeroUuid, result,
              "cpu_async_executor_ is null");
    return result;
  }
  if (io_async_executor_ == nullptr) {
    auto result = FailureExecutionResult(
        SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INITIALIZATION_FAILED);
    SCP_ERROR(kGcpInstanceDatabaseClientProvider, kZeroUuid, result,
              "io_async_executor_ is null");
    return result;
  }

  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceDatabaseClientProvider::Run() noexcept {
  auto project_id_or =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client_);
  if (!project_id_or.Successful()) {
    SCP_ERROR(kGcpInstanceDatabaseClientProvider, kZeroUuid,
              project_id_or.result(),
              "Failed to get project ID for current instance");
    return project_id_or.result();
  }

  auto client_or = gcp_database_factory_->CreateClient(*project_id_or);
  if (!client_or.Successful()) {
    SCP_ERROR(kGcpInstanceDatabaseClientProvider, kZeroUuid, client_or.result(),
              "Failed creating Spanner client");
    return client_or.result();
  }
  spanner_client_shared_ = move(*client_or);

  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceDatabaseClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

void GcpInstanceDatabaseClientProvider::GetInstanceByNameInternal(
    AsyncContext<GetInstanceByNameRequest, GetInstanceByNameResponse>
        get_instance_context,
    string query) noexcept {
  Client spanner_client(*spanner_client_shared_);
  auto row_stream = spanner_client.ExecuteQuery(SqlStatement(move(query)));

  auto row_it = row_stream.begin();
  if (row_it == row_stream.end() || !row_it->ok()) {
    ExecutionResult result = FailureExecutionResult(
        SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND);
    if (!row_it->ok()) {
      result = GcpUtils::GcpErrorConverter(row_it->status());
      SCP_ERROR_CONTEXT(
          kGcpInstanceDatabaseClientProvider, get_instance_context, result,
          "Spanner get instance request failed for Database %s Table %s",
          client_options_->gcp_spanner_database_name.c_str(),
          client_options_->instance_table_name.c_str());
    }
    FinishContext(result, get_instance_context, cpu_async_executor_);
    return;
  }

  auto instance_or =
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(row_it->value());
  if (!instance_or.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceDatabaseClientProvider, get_instance_context,
                      instance_or.result(),
                      "Spanner get instance failed for Database %s Table %s",
                      client_options_->gcp_spanner_database_name.c_str(),
                      client_options_->instance_table_name.c_str());
    FinishContext(instance_or.result(), get_instance_context,
                  cpu_async_executor_);
    return;
  }
  get_instance_context.response = make_shared<GetInstanceByNameResponse>();
  *get_instance_context.response->mutable_instance() = move(*instance_or);

  FinishContext(SuccessExecutionResult(), get_instance_context,
                cpu_async_executor_);
}

void GcpInstanceDatabaseClientProvider::GetInstanceByName(
    AsyncContext<GetInstanceByNameRequest, GetInstanceByNameResponse>&
        get_instance_context) noexcept {
  const auto& request = *get_instance_context.request;
  // Build the query.
  const auto& table_name = client_options_->instance_table_name;
  string query = ConstructInstanceQuery(table_name, kInstanceNameColumnName,
                                        request.instance_name());

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpInstanceDatabaseClientProvider::GetInstanceByNameInternal,
               this, get_instance_context, move(query)),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceDatabaseClientProvider, get_instance_context,
                      schedule_result, "Error scheduling GetInstanceByName");
    get_instance_context.result = schedule_result;
    get_instance_context.Finish();
  }
}

void GcpInstanceDatabaseClientProvider::ListInstancesByStatusInternal(
    AsyncContext<ListInstancesByStatusRequest, ListInstancesByStatusResponse>
        list_instances_context,
    string query) noexcept {
  Client spanner_client(*spanner_client_shared_);
  auto row_stream = spanner_client.ExecuteQuery(SqlStatement(move(query)));

  list_instances_context.response =
      make_shared<ListInstancesByStatusResponse>();
  for (auto row_it = row_stream.begin(); row_it != row_stream.end(); ++row_it) {
    if (!row_it->ok()) {
      auto result = GcpUtils::GcpErrorConverter(row_it->status());
      SCP_ERROR_CONTEXT(
          kGcpInstanceDatabaseClientProvider, list_instances_context, result,
          "Spanner list instances request failed for Database %s Table %s",
          client_options_->gcp_spanner_database_name.c_str(),
          client_options_->instance_table_name.c_str());
      FinishContext(result, list_instances_context, cpu_async_executor_);
      return;
    }

    auto instance_or =
        GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(row_it->value());
    if (!instance_or.Successful()) {
      SCP_ERROR_CONTEXT(kGcpInstanceDatabaseClientProvider,
                        list_instances_context, instance_or.result(),
                        "Spanner get instance failed for Database %s Table %s",
                        client_options_->gcp_spanner_database_name.c_str(),
                        client_options_->instance_table_name.c_str());
      FinishContext(instance_or.result(), list_instances_context,
                    cpu_async_executor_);
      return;
    }
    *list_instances_context.response->add_instances() = move(*instance_or);
  }
  FinishContext(SuccessExecutionResult(), list_instances_context,
                cpu_async_executor_);
}

void GcpInstanceDatabaseClientProvider::ListInstancesByStatus(
    AsyncContext<ListInstancesByStatusRequest, ListInstancesByStatusResponse>&
        list_instances_context) noexcept {
  const auto& request = *list_instances_context.request;
  // Build the query.
  const auto& table_name = client_options_->instance_table_name;
  string query =
      ConstructInstanceQuery(table_name, kInstanceStatusColumnName,
                             InstanceStatus_Name(request.instance_status()));

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(
              &GcpInstanceDatabaseClientProvider::ListInstancesByStatusInternal,
              this, list_instances_context, move(query)),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceDatabaseClientProvider,
                      list_instances_context, schedule_result,
                      "Error scheduling ListInstancesByStatus");
    list_instances_context.result = schedule_result;
    list_instances_context.Finish();
  }
}

void GcpInstanceDatabaseClientProvider::UpdateInstanceInternal(
    AsyncContext<UpdateInstanceRequest, UpdateInstanceResponse>
        update_instance_context) noexcept {
  Client client(*spanner_client_shared_);

  const auto& instance = update_instance_context.request->instance();
  auto commit_result_or = client.Commit(Mutations{
      MakeUpdateMutation(client_options_->instance_table_name,
                         {kInstanceNameColumnName, kInstanceStatusColumnName,
                          kRequestTimeColumnName, kTerminationTimeColumnName},
                         instance.instance_name(),
                         InstanceStatus_Name(instance.status()),
                         MakeTimestamp(instance.request_time()).value(),
                         MakeTimestamp(instance.termination_time()).value()),
  });

  if (!commit_result_or.ok()) {
    ExecutionResult result = RetryExecutionResult(
        SCP_INSTANCE_DATABASE_CLIENT_PROVIDER_COMMIT_FAILED);
    if (commit_result_or.status().code() ==
        google::cloud::StatusCode::kNotFound) {
      result = FailureExecutionResult(
          SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND);
    }
    SCP_ERROR_CONTEXT(
        kGcpInstanceDatabaseClientProvider, update_instance_context, result,
        "Spanner update commit failed. Error code: %d, message: %s",
        commit_result_or.status().code(),
        commit_result_or.status().message().c_str());
    FinishContext(result, update_instance_context, cpu_async_executor_);
    return;
  }
  FinishContext(SuccessExecutionResult(), update_instance_context,
                cpu_async_executor_);
}

void GcpInstanceDatabaseClientProvider::UpdateInstance(
    AsyncContext<UpdateInstanceRequest, UpdateInstanceResponse>&
        update_instance_context) noexcept {
  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpInstanceDatabaseClientProvider::UpdateInstanceInternal, this,
               update_instance_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceDatabaseClientProvider,
                      update_instance_context, schedule_result,
                      "Error scheduling UpdateInstance");
    update_instance_context.result = schedule_result;
    update_instance_context.Finish();
  }
}
}  // namespace google::scp::cpio::client_providers
