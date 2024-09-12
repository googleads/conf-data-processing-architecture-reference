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

#include "job_client.h"

#include <functional>
#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest;
using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse;
using google::cmrt::sdk::job_service::v1::GetJobByIdRequest;
using google::cmrt::sdk::job_service::v1::GetJobByIdResponse;
using google::cmrt::sdk::job_service::v1::GetNextJobRequest;
using google::cmrt::sdk::job_service::v1::GetNextJobResponse;
using google::cmrt::sdk::job_service::v1::PutJobRequest;
using google::cmrt::sdk::job_service::v1::PutJobResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::JobClientProviderFactory;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderFactory;
using google::scp::cpio::client_providers::QueueClientProviderFactory;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using std::placeholders::_1;

namespace {
constexpr char kJobClient[] = "JobClient";
}  // namespace

namespace google::scp::cpio {
ExecutionResultOr<shared_ptr<QueueClientOptions>>
JobClient::CreateQueueClientOptions() noexcept {
  auto queue_options = make_shared<QueueClientOptions>();
  queue_options->queue_name = options_->job_queue_name;
  return queue_options;
}

ExecutionResultOr<shared_ptr<NoSQLDatabaseClientOptions>>
JobClient::CreateNoSQLDatabaseClientOptions() noexcept {
  auto nosql_database_options = make_shared<NoSQLDatabaseClientOptions>();
  nosql_database_options->gcp_spanner_instance_name =
      options_->gcp_spanner_instance_name;
  nosql_database_options->gcp_spanner_database_name =
      options_->gcp_spanner_database_name;
  return nosql_database_options;
}

ExecutionResult JobClient::Init() noexcept {
  shared_ptr<InstanceClientProviderInterface> instance_client;
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(
          GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(
              instance_client)),
      kJobClient, kZeroUuid, "Failed to get InstanceClientProvider.");

  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(
          GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(cpu_async_executor)),
      kJobClient, kZeroUuid, "Failed to get CpuAsyncExecutor.");

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(
          GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor)),
      kJobClient, kZeroUuid, "Failed to get IoAsyncExecutor.");

  auto queue_client_options_or = CreateQueueClientOptions();

  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(queue_client_options_or.result()),
      kJobClient, kZeroUuid, "Failed to create QueueClientOptions.");
  queue_client_provider_ = QueueClientProviderFactory::Create(
      move(*queue_client_options_or), instance_client, cpu_async_executor,
      io_async_executor);
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(queue_client_provider_->Init()),
      kJobClient, kZeroUuid, "Failed to initialize QueueClientProvider.");

  auto nosql_database_client_options_or = CreateNoSQLDatabaseClientOptions();
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(nosql_database_client_options_or.result()),
      kJobClient, kZeroUuid, "Failed to create NoSQLDatabaseClientOptions.");
  nosql_database_client_provider_ = NoSQLDatabaseClientProviderFactory::Create(
      move(*nosql_database_client_options_or), instance_client,
      cpu_async_executor, io_async_executor);
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(nosql_database_client_provider_->Init()),
      kJobClient, kZeroUuid,
      "Failed to initialize NoSQLDatabaseClientProvider.");

  job_client_provider_ = JobClientProviderFactory::Create(
      options_, instance_client, queue_client_provider_,
      nosql_database_client_provider_, cpu_async_executor, io_async_executor);
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(job_client_provider_->Init()), kJobClient,
      kZeroUuid, "Failed to initialize JobClientProvider.");
  return SuccessExecutionResult();
}

ExecutionResult JobClient::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(queue_client_provider_->Run()), kJobClient,
      kZeroUuid, "Failed to run QueueClientProvider.");
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(nosql_database_client_provider_->Run()),
      kJobClient, kZeroUuid, "Failed to run NoSQLDatabaseClientProvider.");
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(job_client_provider_->Run()), kJobClient,
      kZeroUuid, "Failed to run JobClientProvider.");
  return SuccessExecutionResult();
}

ExecutionResult JobClient::Stop() noexcept {
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(job_client_provider_->Stop()), kJobClient,
      kZeroUuid, "Failed to stop JobClientProvider.");
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(nosql_database_client_provider_->Stop()),
      kJobClient, kZeroUuid, "Failed to run NoSQLDatabaseClientProvider.");
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(queue_client_provider_->Stop()),
      kJobClient, kZeroUuid, "Failed to run QueueClientProvider.");
  return SuccessExecutionResult();
}

void JobClient::PutJob(
    AsyncContext<PutJobRequest, PutJobResponse>& put_job_context) noexcept {
  put_job_context.setConvertToPublicError(true);
  job_client_provider_->PutJob(put_job_context);
}

ExecutionResultOr<PutJobResponse> JobClient::PutJobSync(
    PutJobRequest request) noexcept {
  PutJobResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<PutJobRequest, PutJobResponse>(
          bind(&JobClient::PutJob, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kJobClient, kZeroUuid, "Failed to put job.");
  return response;
}

void JobClient::GetNextJob(AsyncContext<GetNextJobRequest, GetNextJobResponse>&
                               get_next_job_context) noexcept {
  get_next_job_context.setConvertToPublicError(true);
  job_client_provider_->GetNextJob(get_next_job_context);
}

ExecutionResultOr<GetNextJobResponse> JobClient::GetNextJobSync(
    GetNextJobRequest request) noexcept {
  GetNextJobResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetNextJobRequest, GetNextJobResponse>(
          bind(&JobClient::GetNextJob, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kJobClient, kZeroUuid, "Failed to get next job.");
  return response;
}

void JobClient::GetJobById(AsyncContext<GetJobByIdRequest, GetJobByIdResponse>&
                               get_job_by_id_context) noexcept {
  get_job_by_id_context.setConvertToPublicError(true);
  job_client_provider_->GetJobById(get_job_by_id_context);
}

ExecutionResultOr<GetJobByIdResponse> JobClient::GetJobByIdSync(
    GetJobByIdRequest request) noexcept {
  GetJobByIdResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetJobByIdRequest, GetJobByIdResponse>(
          bind(&JobClient::GetJobById, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kJobClient, kZeroUuid, "Failed to get job by ID.");
  return response;
}

void JobClient::UpdateJobBody(
    AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse>&
        update_job_body_context) noexcept {
  update_job_body_context.setConvertToPublicError(true);
  job_client_provider_->UpdateJobBody(update_job_body_context);
}

ExecutionResultOr<UpdateJobBodyResponse> JobClient::UpdateJobBodySync(
    UpdateJobBodyRequest request) noexcept {
  UpdateJobBodyResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<UpdateJobBodyRequest, UpdateJobBodyResponse>(
          bind(&JobClient::UpdateJobBody, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kJobClient, kZeroUuid,
                            "Failed to update job body.");
  return response;
}

void JobClient::UpdateJobStatus(
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context) noexcept {
  update_job_status_context.setConvertToPublicError(true);
  job_client_provider_->UpdateJobStatus(update_job_status_context);
}

ExecutionResultOr<UpdateJobStatusResponse> JobClient::UpdateJobStatusSync(
    UpdateJobStatusRequest request) noexcept {
  UpdateJobStatusResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<UpdateJobStatusRequest, UpdateJobStatusResponse>(
          bind(&JobClient::UpdateJobStatus, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kJobClient, kZeroUuid,
                            "Failed to update job status.");
  return response;
}

void JobClient::UpdateJobVisibilityTimeout(
    AsyncContext<UpdateJobVisibilityTimeoutRequest,
                 UpdateJobVisibilityTimeoutResponse>&
        update_job_visibility_timeout_context) noexcept {
  update_job_visibility_timeout_context.setConvertToPublicError(true);
  job_client_provider_->UpdateJobVisibilityTimeout(
      update_job_visibility_timeout_context);
}

ExecutionResultOr<UpdateJobVisibilityTimeoutResponse>
JobClient::UpdateJobVisibilityTimeoutSync(
    UpdateJobVisibilityTimeoutRequest request) noexcept {
  UpdateJobVisibilityTimeoutResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<UpdateJobVisibilityTimeoutRequest,
                              UpdateJobVisibilityTimeoutResponse>(
          bind(&JobClient::UpdateJobVisibilityTimeout, this, _1), move(request),
          response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kJobClient, kZeroUuid,
                            "Failed to update job visibility timeout.");
  return response;
}

void JobClient::DeleteOrphanedJobMessage(
    AsyncContext<DeleteOrphanedJobMessageRequest,
                 DeleteOrphanedJobMessageResponse>&
        delete_orphaned_job_context) noexcept {
  delete_orphaned_job_context.setConvertToPublicError(true);
  job_client_provider_->DeleteOrphanedJobMessage(delete_orphaned_job_context);
}

ExecutionResultOr<DeleteOrphanedJobMessageResponse>
JobClient::DeleteOrphanedJobMessageSync(
    DeleteOrphanedJobMessageRequest request) noexcept {
  DeleteOrphanedJobMessageResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<DeleteOrphanedJobMessageRequest,
                              DeleteOrphanedJobMessageResponse>(
          bind(&JobClient::DeleteOrphanedJobMessage, this, _1), move(request),
          response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kJobClient, kZeroUuid,
                            "Failed to delete orphaned job message.");
  return response;
}

unique_ptr<JobClientInterface> JobClientFactory::Create(
    JobClientOptions options) noexcept {
  return make_unique<JobClient>(make_shared<JobClientOptions>(options));
}
}  // namespace google::scp::cpio
