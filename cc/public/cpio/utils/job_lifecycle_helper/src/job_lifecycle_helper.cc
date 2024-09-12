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

#include "job_lifecycle_helper.h"

#include <map>
#include <string>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "core/interface/async_context.h"
#include "cpio/client_providers/job_client_provider/src/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/utils/job_lifecycle_helper/proto/v1/job_lifecycle_helper.pb.h"
#include "public/cpio/utils/metric_instance/interface/metric_instance_factory_interface.h"
#include "public/cpio/utils/metric_instance/src/metric_utils.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

#include "error_codes.h"

using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationResponse;
using google::cmrt::sdk::job_lifecycle_helper::v1::JobMessageMetadata;
using google::cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest;
using google::cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse;
using google::cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest;
using google::cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse;
using google::cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest;
using google::cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse;
using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest;
using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse;
using google::cmrt::sdk::job_service::v1::GetJobByIdRequest;
using google::cmrt::sdk::job_service::v1::GetJobByIdResponse;
using google::cmrt::sdk::job_service::v1::GetNextJobRequest;
using google::cmrt::sdk::job_service::v1::GetNextJobResponse;
using google::cmrt::sdk::job_service::v1::Job;
using google::cmrt::sdk::job_service::v1::JobStatus;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::protobuf::Duration;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST;
using google::scp::core::errors::SC_CPIO_ENTITY_NOT_FOUND;
using google::scp::core::errors::
    SC_JOB_LIFECYCLE_HELPER_CURRENT_INSTANCE_IS_TERMINATING;
using google::scp::core::errors::
    SC_JOB_LIFECYCLE_HELPER_INVALID_DURATION_BEFORE_RELEASE;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_INVALID_JOB_STATUS;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_JOB_ALREADY_COMPLETED;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_JOB_BEING_PROCESSING;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_MISSING_JOB_ID;
using google::scp::core::errors::
    SC_JOB_LIFECYCLE_HELPER_MISSING_METRIC_INSTANCE_FACTORY;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_MISSING_RECEIPT_INFO;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_ORPHANED_JOB_FOUND;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_RETRY_EXHAUSTED;
using google::scp::cpio::AggregateMetricInterface;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricUtils;
using google::scp::cpio::SimpleMetricInterface;
using google::scp::cpio::TimeEvent;
using std::bind;
using std::make_shared;
using std::map;
using std::nullopt;
using std::optional;
using std::reference_wrapper;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;

namespace {
constexpr char kJobLifecycleHelper[] = "JobLifecycleHelper";
constexpr char kJobPreparationMethodName[] = "JobPreparation";
constexpr char kJobPreparationMetricName[] = "JobPreparationCount";
constexpr char kJobPreparationEventName[] = "JobPreparation";
constexpr char kJobPreparationSuccessEventName[] = "JobPreparationSuccess";
constexpr char kJobPreparationFailureMetricName[] = "JobPreparationFailure";
constexpr char kJobPreparationTryFinishInstanceTerminationFailureEventName[] =
    "JobPreparationTryFinishInstanceTerminationFailure";
constexpr char kJobPreparationCurrentInstanceTerminationFailureEventName[] =
    "JobPreparationCurrentInstanceTerminationFailure";
constexpr char kJobPreparationGetNextJobFailureEventName[] =
    "JobPreparationGetNextJobFailure";
constexpr char kJobPreparationUpdateJobStatusFailureEventName[] =
    "JobPreparationUpdateJobStatusFailure";
constexpr char kJobCompletionMethodName[] = "JobCompletion";
constexpr char kJobCompletionMetricName[] = "JobCompletionCount";
constexpr char kJobCompletionSuccessEventName[] = "JobCompletionSuccess";
constexpr char kJobCompletionFailureMetricName[] = "JobCompletionFailure";
constexpr char kJobCompletionInvalidMarkJobCompletedFailureEventName[] =
    "JobCompletionInvalidMarkJobCompletedFailure";
constexpr char kJobCompletionGetJobByIdFailureEventName[] =
    "JobCompletionGetJobByIdFailure";
constexpr char kJobCompletionUpdateJobStatusFailureEventName[] =
    "JobCompletionUpdateJobStatusFailure";
constexpr char kJobCompletionJobStatusFailureEventName[] =
    "JobCompletionJobStatusFailure";
constexpr char kJobProcessingTimeErrorEventName[] = "JobProcessingTimeError";
constexpr char kJobReleaseMethodName[] = "JobRelease";
constexpr char kJobReleaseMetricName[] = "JobReleaseCount";
constexpr char kJobReleaseEventName[] = "JobRelease";
constexpr char kJobReleaseSuccessEventName[] = "JobReleaseSuccess";
constexpr char kJobReleaseFailureMetricName[] = "JobReleaseFailure";
constexpr char kJobReleaseInvalidReleaseJobForRetryFailureEventName[] =
    "JobReleaseInvalidReleaseJobForRetryFailure";
constexpr char kJobReleaseGetJobByIdFailureEventName[] =
    "JobReleaseGetJobByIdFailure";
constexpr char kJobReleaseInvalidJobStatusFailureEventName[] =
    "JobReleaseInvalidJobStatusFailure";
constexpr char kJobReleaseUpdateJobStatusFailureEventName[] =
    "JobReleaseUpdateJobStatusFailure";
constexpr char kJobReleaseUpdateJobVisibilityTimeoutFailureEventName[] =
    "JobReleaseUpdateJobVisibilityTimeoutFailure";
constexpr char kJobWaitingTimeMethodName[] = "JobWaitingTime";
constexpr char kJobWaitingTimeMetricName[] = "JobWaitingTimeCount";
constexpr char kJobProcessingTimeMethodName[] = "JobProcessingTime";
constexpr char kJobProcessingTimeMetricName[] = "JobProcessingTimeCount";
constexpr char kJobExtenderMethodName[] = "JobExtender";
constexpr char kJobExtenderFailureEventName[] = "JobExtenderFailure";
constexpr char kJobExtenderGetJobByIdFailureEventName[] =
    "JobExtenderGetJobByIdFailure";
constexpr char kJobExtenderUpdateJobVisibilityTimeoutFailureEventName[] =
    "JobExtenderUpdateJobVisibilityTimeoutFailure";
constexpr char kJobMetadataMapMethodName[] = "JobMetadataMap";
constexpr char kJobMetadataMapFailureMetricName[] = "JobMetadataMapFailure";
constexpr char kJobMetadataMapUpdateJobMetadataFailureEventName[] =
    "JobMetadataMapUpdateJobMetadataFailure";
constexpr char kJobMetadataMapDeleteJobMetadataFailureEventName[] =
    "JobMetadataMapDeleteJobMetadataFailure";
constexpr char kJobMetadataMapFindJobMetadataFailureEventName[] =
    "JobMetadataMapFindJobMetadataFailure";
constexpr char kJobMetadataMapInsertJobMetadataFailureEventName[] =
    "JobMetadataMapInsertJobMetadataFailure";
constexpr char kJobMetadataMapMissingReceiptInfoFailureEventName[] =
    "JobMetadataMapMissingReceiptInfoFailure";

constexpr int64_t kDefaultTimestampInSeconds = 0;
constexpr int64_t kMaximumVisibilityTimeoutInSeconds = 600;
}  // namespace

namespace google::scp::cpio {

ExecutionResult JobLifecycleHelper::Init() noexcept {
  RETURN_IF_FAILURE(InitAllMetrics());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::Run() noexcept {
  RETURN_IF_FAILURE(RunAllMetrics());
  is_running_ = true;
  job_extender_worker_ = std::make_unique<std::thread>(
      [this]() mutable { StartJobExtenderThread(); });
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::Stop() noexcept {
  is_running_ = false;
  if (job_extender_worker_->joinable()) {
    job_extender_worker_->join();
  }
  RETURN_IF_FAILURE(StopAllMetrics());
  return SuccessExecutionResult();
}

void JobLifecycleHelper::PrepareNextJob(
    AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>
        prepare_job_context) noexcept {
  auto try_finish_instance_termination_request =
      make_shared<TryFinishInstanceTerminationRequest>();
  try_finish_instance_termination_request->set_instance_resource_name(
      options_.current_instance_resource_name());
  try_finish_instance_termination_request->set_scale_in_hook_name(
      options_.scale_in_hook_name());

  AsyncContext<TryFinishInstanceTerminationRequest,
               TryFinishInstanceTerminationResponse>
      try_finish_instance_termination_context(
          std::move(try_finish_instance_termination_request),
          bind(&JobLifecycleHelper::TryFinishInstanceTerminationCallback, this,
               prepare_job_context, _1),
          prepare_job_context);

  auto_scaling_client_->TryFinishInstanceTermination(
      try_finish_instance_termination_context);
}

void JobLifecycleHelper::TryFinishInstanceTerminationCallback(
    AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
        prepare_job_context,
    AsyncContext<TryFinishInstanceTerminationRequest,
                 TryFinishInstanceTerminationResponse>&
        try_finish_instance_termination_context) noexcept {
  auto result = try_finish_instance_termination_context.result;
  if (!result.Successful()) {
    IncrementAggregateMetric(
        job_preparation_metric_,
        kJobPreparationTryFinishInstanceTerminationFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                      "Failed to prepare job due to try finish instance "
                      "termination failed.");
    prepare_job_context.result = result;
    prepare_job_context.Finish();
    return;
  }

  // If the current instance is scheduled for termination, exit.
  if (try_finish_instance_termination_context.response
          ->termination_scheduled()) {
    IncrementAggregateMetric(
        job_preparation_failure_metric_,
        kJobPreparationCurrentInstanceTerminationFailureEventName);
    result = FailureExecutionResult(
        SC_JOB_LIFECYCLE_HELPER_CURRENT_INSTANCE_IS_TERMINATING);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                      "Failed to prepare job due to current instance is "
                      "scheduled for termination.");
    prepare_job_context.result = result;
    prepare_job_context.Finish();
    return;
  }

  AsyncContext<GetNextJobRequest, GetNextJobResponse> get_next_job_context(
      std::move(make_shared<GetNextJobRequest>()),
      bind(&JobLifecycleHelper::GetNextJobCallback, this, prepare_job_context,
           _1),
      prepare_job_context);

  job_client_->GetNextJob(get_next_job_context);
}

void JobLifecycleHelper::GetNextJobCallback(
    AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
        prepare_job_context,
    AsyncContext<GetNextJobRequest, GetNextJobResponse>&
        get_next_job_context) noexcept {
  auto result = get_next_job_context.result;
  if (!result.Successful()) {
    if (result.status_code != SC_CPIO_ENTITY_NOT_FOUND) {
      IncrementAggregateMetric(job_preparation_failure_metric_,
                               kJobPreparationGetNextJobFailureEventName);
      SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                        "Failed to prepare job due to get next job failed");
    } else {
      SCP_DEBUG_CONTEXT(kJobLifecycleHelper, prepare_job_context,
                        "Failed to prepare job due to entity not found");
    }
    prepare_job_context.result = result;
    prepare_job_context.Finish();
    return;
  }

  IncrementAggregateMetric(job_preparation_metric_, kJobPreparationEventName);

  const string& job_id = get_next_job_context.response->job().job_id();
  const JobStatus job_status =
      get_next_job_context.response->job().job_status();

  // Orphaned job will have default vales in its fields, here we check the
  // job status and created time. Remove these orphaned job messages in job
  // client before returning failure.
  auto created_time = get_next_job_context.response->job().created_time();
  if (job_status == JobStatus::JOB_STATUS_UNKNOWN &&
      created_time.seconds() == kDefaultTimestampInSeconds) {
    result = FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_ORPHANED_JOB_FOUND);
    SCP_ERROR_CONTEXT(
        kJobLifecycleHelper, prepare_job_context, result,
        "Failed to prepare job due to job is orphaned. Job id: %s",
        job_id.c_str());
    prepare_job_context.result = result;
    DeleteOrphanedJobMessage(prepare_job_context, get_next_job_context);
    return;
  }

  if (job_status == JobStatus::JOB_STATUS_PROCESSING &&
      !ExceedingProcessingTimeout(job_id, get_next_job_context.response->job()
                                              .processing_started_time()
                                              .seconds())) {
    result =
        FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_JOB_BEING_PROCESSING);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                      "Failed to prepare job due to job is already being "
                      "processing by another worker. Job id: %s",
                      job_id.c_str());
    prepare_job_context.result = result;
    prepare_job_context.Finish();
    return;
  }

  if (job_status == JobStatus::JOB_STATUS_SUCCESS ||
      job_status == JobStatus::JOB_STATUS_FAILURE) {
    result =
        FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_JOB_ALREADY_COMPLETED);
    SCP_ERROR_CONTEXT(
        kJobLifecycleHelper, prepare_job_context, result,
        "Failed to prepare job due to job is already completed. Job id: %s",
        job_id.c_str());
    // Remove orphaned job message (As they are already completed) in job
    // client before returning failure.
    prepare_job_context.result = result;
    DeleteOrphanedJobMessage(prepare_job_context, get_next_job_context);
    return;
  }

  int retry_count = get_next_job_context.response->job().retry_count();
  if (retry_count >= options_.retry_limit()) {
    auto update_job_status_request = make_shared<UpdateJobStatusRequest>();
    update_job_status_request->set_job_id(job_id);
    update_job_status_request->set_job_status(JobStatus::JOB_STATUS_FAILURE);
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>
        update_job_status_context(
            std::move(update_job_status_request),
            bind(&JobLifecycleHelper::UpdateJobStatusCallbackForPrepareJob,
                 this, prepare_job_context, _1),
            prepare_job_context);
    job_client_->UpdateJobStatus(update_job_status_context);
    return;
  }

  JobMessageMetadata job_message_metadata;
  job_message_metadata.set_job_id(job_id);
  job_message_metadata.set_receipt_info(
      get_next_job_context.response->receipt_info());
  job_message_metadata.set_is_visibility_timeout_extendable(
      prepare_job_context.request->is_visibility_timeout_extendable());
  result =
      InsertJobMessageMetadataToMap(prepare_job_context, job_message_metadata);
  if (!result.Successful()) {
    return;
  }

  IncrementAggregateMetric(job_preparation_metric_,
                           kJobPreparationSuccessEventName);
  prepare_job_context.response = make_shared<PrepareNextJobResponse>();
  prepare_job_context.response->set_allocated_job(
      get_next_job_context.response->release_job());
  prepare_job_context.result = result;
  prepare_job_context.Finish();
}

void JobLifecycleHelper::DeleteOrphanedJobMessage(
    AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
        prepare_job_context,
    AsyncContext<GetNextJobRequest, GetNextJobResponse>&
        get_next_job_context) noexcept {
  const string& job_id = get_next_job_context.response->job().job_id();

  auto delete_orphaned_job_message_request =
      make_shared<DeleteOrphanedJobMessageRequest>();
  delete_orphaned_job_message_request->set_job_id(job_id);
  delete_orphaned_job_message_request->set_receipt_info(
      get_next_job_context.response->receipt_info());

  AsyncContext<DeleteOrphanedJobMessageRequest,
               DeleteOrphanedJobMessageResponse>
      delete_orphaned_job_message_context(
          std::move(delete_orphaned_job_message_request),
          bind(&JobLifecycleHelper::DeleteOrphanedJobMessageCallback, this,
               prepare_job_context, _1),
          prepare_job_context);

  job_client_->DeleteOrphanedJobMessage(delete_orphaned_job_message_context);
}

void JobLifecycleHelper::DeleteOrphanedJobMessageCallback(
    AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
        prepare_job_context,
    AsyncContext<DeleteOrphanedJobMessageRequest,
                 DeleteOrphanedJobMessageResponse>&
        delete_orphaned_job_message_context) noexcept {
  const string& job_id = delete_orphaned_job_message_context.request->job_id();
  auto result = delete_orphaned_job_message_context.result;
  if (!result.Successful()) {
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                      "Failed to prepare job due to delete orphaned job "
                      "message failure. Job id: %s",
                      job_id.c_str());
    prepare_job_context.result = result;
    prepare_job_context.Finish();
    return;
  }

  result = DeleteJobMetadataFromMap(job_id);
  if (!result.Successful() &&
      result.status_code != SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
    IncrementAggregateMetric(job_metadata_map_failure_metric_,
                             kJobMetadataMapDeleteJobMetadataFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                      "Failed to remove job message metadata from the "
                      "map. Job id: %s",
                      job_id.c_str());
    prepare_job_context.result = result;
  }
  prepare_job_context.Finish();
}

ExecutionResult JobLifecycleHelper::InsertJobMessageMetadataToMap(
    AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
        prepare_job_context,
    JobMessageMetadata updated_job_message_metadata) noexcept {
  const string& job_id = updated_job_message_metadata.job_id();
  auto job_metadata_in_map_or = FindJobMetadataById(job_id);
  if (!job_metadata_in_map_or.Successful()) {
    // Continue if the job entry is not found in the map because it's expected
    // when the job is first introduced to the map.
    // Only return if other error occurs.
    auto result = job_metadata_in_map_or.result();
    if (result.status_code != SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
      SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                        "Failed to prepare job due to finding corresponding "
                        "job message metadata in the map failed. Job id: %s",
                        job_id.c_str());
      prepare_job_context.result = result;
      prepare_job_context.Finish();
      return result;
    }
  } else {
    // Always erase the existing job entry from the map before insertion.
    auto result = DeleteJobMetadataFromMap(job_id);
    if (!result.Successful() &&
        result.status_code != SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
      IncrementAggregateMetric(
          job_metadata_map_failure_metric_,
          kJobMetadataMapDeleteJobMetadataFailureEventName);
      SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                        "Failed to remove job message metadata from the "
                        "map. Job id: %s",
                        job_id.c_str());
      prepare_job_context.result = result;
      prepare_job_context.Finish();
      return result;
    }
  }

  auto job_message_metadata_key_pair =
      make_pair(job_id, updated_job_message_metadata);
  auto result = job_message_metadata_map_.Insert(job_message_metadata_key_pair,
                                                 updated_job_message_metadata);
  if (!result.Successful()) {
    IncrementAggregateMetric(job_metadata_map_failure_metric_,
                             kJobMetadataMapInsertJobMetadataFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                      "Failed to prepare job due to inserting job message "
                      "metadata to the map failed. Job id: %s",
                      job_id.c_str());
    prepare_job_context.result = result;
    prepare_job_context.Finish();
    return result;
  }

  return SuccessExecutionResult();
}

void JobLifecycleHelper::UpdateJobStatusCallbackForPrepareJob(
    AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
        prepare_job_context,
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context) noexcept {
  const string& job_id = update_job_status_context.request->job_id();
  auto result = update_job_status_context.result;
  if (!result.Successful()) {
    IncrementAggregateMetric(job_preparation_failure_metric_,
                             kJobPreparationUpdateJobStatusFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, prepare_job_context, result,
                      "Failed to prepare job due to update job status "
                      "failure. Job id: %s",
                      job_id.c_str());
    prepare_job_context.result = result;
    prepare_job_context.Finish();
    return;
  }

  prepare_job_context.result =
      FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_RETRY_EXHAUSTED);
  prepare_job_context.Finish();
}

ExecutionResultOr<PrepareNextJobResponse>
JobLifecycleHelper::PrepareNextJobSync(PrepareNextJobRequest request) noexcept {
  PrepareNextJobResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<PrepareNextJobRequest, PrepareNextJobResponse>(
          bind(&JobLifecycleHelper::PrepareNextJob, this, _1),
          std::move(request), response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kJobLifecycleHelper, kZeroUuid,
                            "Failed to prepare next job.");
  return response;
}

void JobLifecycleHelper::MarkJobCompleted(
    AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>
        mark_job_completed_context) noexcept {
  const string& job_id = mark_job_completed_context.request->job_id();
  if (job_id.empty()) {
    IncrementAggregateMetric(
        job_completion_failure_metric_,
        kJobCompletionInvalidMarkJobCompletedFailureEventName);
    auto execution_result =
        FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_MISSING_JOB_ID);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, mark_job_completed_context,
                      execution_result,
                      "Failed to mark job completed due to missing job id.");
    mark_job_completed_context.result = execution_result;
    mark_job_completed_context.Finish();
    return;
  }

  const JobStatus job_status = mark_job_completed_context.request->job_status();
  if (job_status != JobStatus::JOB_STATUS_SUCCESS &&
      job_status != JobStatus::JOB_STATUS_FAILURE) {
    IncrementAggregateMetric(
        job_completion_failure_metric_,
        kJobCompletionInvalidMarkJobCompletedFailureEventName);
    auto execution_result =
        FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_INVALID_JOB_STATUS);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, mark_job_completed_context,
                      execution_result,
                      "Failed to mark job completed due to invalid job status. "
                      "Job id: %s, job status in request: %s",
                      job_id.c_str(), JobStatus_Name(job_status).c_str());
    mark_job_completed_context.result = execution_result;
    mark_job_completed_context.Finish();
    return;
  }

  auto get_job_by_id_request = make_shared<GetJobByIdRequest>();
  get_job_by_id_request->set_job_id(job_id);
  AsyncContext<GetJobByIdRequest, GetJobByIdResponse> get_job_by_id_context(
      std::move(get_job_by_id_request),
      bind(&JobLifecycleHelper::GetJobByIdCallbackForMarkJobCompleted, this,
           mark_job_completed_context, _1),
      mark_job_completed_context);

  job_client_->GetJobById(get_job_by_id_context);
}

void JobLifecycleHelper::GetJobByIdCallbackForMarkJobCompleted(
    AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>&
        mark_job_completed_context,
    AsyncContext<GetJobByIdRequest, GetJobByIdResponse>&
        get_job_by_id_context) noexcept {
  const string& job_id = mark_job_completed_context.request->job_id();

  auto result = get_job_by_id_context.result;
  if (!result.Successful()) {
    IncrementAggregateMetric(job_completion_failure_metric_,
                             kJobCompletionGetJobByIdFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, mark_job_completed_context, result,
                      "Failed to mark job completed due to get job by id "
                      "failed. Job id: %s",
                      job_id.c_str());
    mark_job_completed_context.result = result;
    mark_job_completed_context.Finish();
    return;
  }

  auto job_message_metadata_or = FindJobMetadataById(job_id);
  if (!job_message_metadata_or.Successful()) {
    IncrementAggregateMetric(job_metadata_map_failure_metric_,
                             kJobMetadataMapFindJobMetadataFailureEventName);
    result = job_message_metadata_or.result();
    SCP_ERROR_CONTEXT(
        kJobLifecycleHelper, mark_job_completed_context, result,
        "Failed to mark job completed due to finding corresponding "
        "job message metadata in the map failed. Job id: %s",
        job_id.c_str());
    mark_job_completed_context.result = result;
    mark_job_completed_context.Finish();
    return;
  }

  auto update_job_status_request = make_shared<UpdateJobStatusRequest>();
  update_job_status_request->set_job_id(job_id);
  update_job_status_request->set_job_status(
      mark_job_completed_context.request->job_status());
  update_job_status_request->set_receipt_info(
      job_message_metadata_or.value().receipt_info());
  *update_job_status_request->mutable_most_recent_updated_time() =
      get_job_by_id_context.response->job().updated_time();

  auto processing_started_time = make_shared<google::protobuf::Timestamp>(
      get_job_by_id_context.response->job().processing_started_time());
  auto created_time = make_shared<google::protobuf::Timestamp>(
      get_job_by_id_context.response->job().created_time());
  AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>
      update_job_status_context(
          std::move(update_job_status_request),
          bind(&JobLifecycleHelper::UpdateJobStatusCallbackForMarkJobCompleted,
               this, mark_job_completed_context,
               std::move(processing_started_time), std::move(created_time), _1),
          mark_job_completed_context);

  job_client_->UpdateJobStatus(update_job_status_context);
}

void JobLifecycleHelper::UpdateJobStatusCallbackForMarkJobCompleted(
    AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>&
        mark_job_completed_context,
    shared_ptr<google::protobuf::Timestamp> processing_started_time,
    shared_ptr<google::protobuf::Timestamp> created_time,
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context) noexcept {
  const string& job_id = update_job_status_context.request->job_id();
  auto result = update_job_status_context.result;
  if (!result.Successful()) {
    IncrementAggregateMetric(job_completion_failure_metric_,
                             kJobCompletionUpdateJobStatusFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, mark_job_completed_context, result,
                      "Failed to mark job completed due to update job status "
                      "failure. Job id: %s",
                      job_id.c_str());
    mark_job_completed_context.result = result;
    mark_job_completed_context.Finish();
    return;
  }

  result = DeleteJobMetadataFromMap(job_id);
  if (!result.Successful() &&
      result.status_code != SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
    IncrementAggregateMetric(job_metadata_map_failure_metric_,
                             kJobMetadataMapDeleteJobMetadataFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, mark_job_completed_context, result,
                      "Failed to remove job message metadata from the "
                      "map. Job id: %s",
                      job_id.c_str());
    mark_job_completed_context.result = result;
    mark_job_completed_context.Finish();
    return;
  }

  auto completed_time_in_milliseconds = TimeUtil::TimestampToMilliseconds(
      update_job_status_context.response->updated_time());
  auto processing_started_time_in_milliseconds =
      TimeUtil::TimestampToMilliseconds(*processing_started_time);
  auto processing_time_in_milliseconds =
      completed_time_in_milliseconds - processing_started_time_in_milliseconds;
  if (processing_time_in_milliseconds < 0) {
    SCP_ERROR_CONTEXT(
        kJobLifecycleHelper, mark_job_completed_context, result,
        "Completed time: %d is less than processing started time: %d."
        " Job id: %s",
        completed_time_in_milliseconds, processing_started_time_in_milliseconds,
        job_id.c_str());
    IncrementAggregateMetric(job_completion_failure_metric_,
                             kJobProcessingTimeErrorEventName);
  } else {
    RecordTimeInSimpleMetric(job_processing_time_metric_,
                             to_string(processing_time_in_milliseconds));
  }
  auto created_time_in_milliseconds =
      TimeUtil::TimestampToMilliseconds(*created_time);
  auto waiting_time_in_milliseconds =
      processing_started_time_in_milliseconds - created_time_in_milliseconds;
  if (waiting_time_in_milliseconds < 0) {
    SCP_ERROR_CONTEXT(
        kJobLifecycleHelper, mark_job_completed_context, result,
        "Processing started time: %d is less than created time: %d."
        " Job id: %s",
        processing_started_time_in_milliseconds, created_time_in_milliseconds,
        job_id.c_str());
    IncrementAggregateMetric(job_completion_failure_metric_,
                             kJobProcessingTimeErrorEventName);
  } else {
    RecordTimeInSimpleMetric(job_waiting_time_metric_,
                             to_string(waiting_time_in_milliseconds));
  }
  IncrementAggregateMetric(job_completion_metric_,
                           kJobCompletionSuccessEventName);
  if (mark_job_completed_context.request->job_status() ==
      JobStatus::JOB_STATUS_FAILURE) {
    IncrementAggregateMetric(job_completion_metric_,
                             kJobCompletionJobStatusFailureEventName);
  }
  mark_job_completed_context.response = make_shared<MarkJobCompletedResponse>();
  mark_job_completed_context.result = SuccessExecutionResult();
  mark_job_completed_context.Finish();
}

ExecutionResultOr<MarkJobCompletedResponse>
JobLifecycleHelper::MarkJobCompletedSync(
    MarkJobCompletedRequest request) noexcept {
  MarkJobCompletedResponse response;
  auto execution_result = SyncUtils::AsyncToSync2<MarkJobCompletedRequest,
                                                  MarkJobCompletedResponse>(
      bind(&JobLifecycleHelper::MarkJobCompleted, this, _1), std::move(request),
      response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kJobLifecycleHelper, kZeroUuid,
                            "Failed to mark job completed.");
  return response;
}

void JobLifecycleHelper::ReleaseJobForRetry(
    AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>
        release_job_for_retry_context) noexcept {
  const string& job_id = release_job_for_retry_context.request->job_id();
  if (job_id.empty()) {
    IncrementAggregateMetric(
        job_release_failure_metric_,
        kJobReleaseInvalidReleaseJobForRetryFailureEventName);
    auto result =
        FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_MISSING_JOB_ID);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, release_job_for_retry_context,
                      result,
                      "Failed to release job for retry due to missing job id.");
    release_job_for_retry_context.result = result;
    release_job_for_retry_context.Finish();
    return;
  }

  const auto& duration_before_release =
      release_job_for_retry_context.request->duration_before_release()
          .seconds();
  if (duration_before_release < kDefaultTimestampInSeconds ||
      duration_before_release > kMaximumVisibilityTimeoutInSeconds) {
    IncrementAggregateMetric(
        job_release_failure_metric_,
        kJobReleaseInvalidReleaseJobForRetryFailureEventName);
    auto result = FailureExecutionResult(
        SC_JOB_LIFECYCLE_HELPER_INVALID_DURATION_BEFORE_RELEASE);
    SCP_ERROR_CONTEXT(
        kJobLifecycleHelper, release_job_for_retry_context, result,
        "Failed to release job for retry due to invalid duration "
        "before release. Job id: %s, duration before release in request: %d",
        job_id.c_str(), duration_before_release);
    release_job_for_retry_context.result = result;
    release_job_for_retry_context.Finish();
    return;
  }

  auto job_message_metadata_or = FindJobMetadataById(job_id);
  if (!job_message_metadata_or.Successful()) {
    IncrementAggregateMetric(job_metadata_map_failure_metric_,
                             kJobMetadataMapFindJobMetadataFailureEventName);
    auto result = job_message_metadata_or.result();
    SCP_ERROR(kJobLifecycleHelper, kZeroUuid, result,
              "Failed to release job for retry due to finding corresponding "
              "job message metadata in the map failed. Job id: %s",
              job_id.c_str());
    release_job_for_retry_context.result = result;
    release_job_for_retry_context.Finish();
    return;
  }

  IncrementAggregateMetric(job_release_metric_, kJobReleaseEventName);

  auto get_job_by_id_request = make_shared<GetJobByIdRequest>();
  get_job_by_id_request->set_job_id(job_id);
  AsyncContext<GetJobByIdRequest, GetJobByIdResponse> get_job_by_id_context(
      std::move(get_job_by_id_request),
      bind(&JobLifecycleHelper::GetJobByIdCallbackForReleaseJobForRetry, this,
           release_job_for_retry_context,
           make_shared<JobMessageMetadata>(*job_message_metadata_or), _1),
      release_job_for_retry_context);

  job_client_->GetJobById(get_job_by_id_context);
}

void JobLifecycleHelper::GetJobByIdCallbackForReleaseJobForRetry(
    AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
        release_job_for_retry_context,
    shared_ptr<JobMessageMetadata> job_message_metadata,
    AsyncContext<GetJobByIdRequest, GetJobByIdResponse>&
        get_job_by_id_context) noexcept {
  const string& job_id = release_job_for_retry_context.request->job_id();

  auto result = get_job_by_id_context.result;
  if (!result.Successful()) {
    IncrementAggregateMetric(job_release_failure_metric_,
                             kJobReleaseGetJobByIdFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, release_job_for_retry_context,
                      result,
                      "Failed to release job for retry due to get job by id "
                      "failed. Job id: %s",
                      job_id.c_str());
    release_job_for_retry_context.result = result;
    release_job_for_retry_context.Finish();
    return;
  }

  const auto& job_status = get_job_by_id_context.response->job().job_status();
  if (job_status != JobStatus::JOB_STATUS_CREATED &&
      job_status != JobStatus::JOB_STATUS_PROCESSING) {
    result = FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_INVALID_JOB_STATUS);
    IncrementAggregateMetric(job_release_failure_metric_,

                             kJobReleaseInvalidJobStatusFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, release_job_for_retry_context,
                      result,
                      "Failed to release job for retry due to invalid job "
                      "status. Job id: %s, job status in request: %s",
                      job_id.c_str(), JobStatus_Name(job_status).c_str());
    release_job_for_retry_context.result = result;

    auto result = DeleteJobMetadataFromMap(job_id);
    if (!result.Successful() &&
        result.status_code != SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
      IncrementAggregateMetric(
          job_metadata_map_failure_metric_,
          kJobMetadataMapDeleteJobMetadataFailureEventName);
      SCP_ERROR_CONTEXT(kJobLifecycleHelper, release_job_for_retry_context,
                        result,
                        "Failed to remove job message metadata from the "
                        "map. Job id: %s",
                        job_id.c_str());
      release_job_for_retry_context.result = result;
    }
    release_job_for_retry_context.Finish();
    return;
  }

  auto update_job_status_request = make_shared<UpdateJobStatusRequest>();
  update_job_status_request->set_job_id(job_id);
  update_job_status_request->set_job_status(JobStatus::JOB_STATUS_CREATED);
  *update_job_status_request->mutable_most_recent_updated_time() =
      get_job_by_id_context.response->job().updated_time();

  AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>
      update_job_status_context(
          std::move(update_job_status_request),
          bind(
              &JobLifecycleHelper::UpdateJobStatusCallbackForReleaseJobForRetry,
              this, release_job_for_retry_context,
              std::move(job_message_metadata), _1),
          release_job_for_retry_context);

  job_client_->UpdateJobStatus(update_job_status_context);
}

void JobLifecycleHelper::UpdateJobStatusCallbackForReleaseJobForRetry(
    AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
        release_job_for_retry_context,
    shared_ptr<JobMessageMetadata> job_message_metadata,
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context) noexcept {
  const string& job_id = update_job_status_context.request->job_id();
  auto result = update_job_status_context.result;
  if (!result.Successful()) {
    IncrementAggregateMetric(job_release_failure_metric_,

                             kJobReleaseUpdateJobStatusFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, release_job_for_retry_context,
                      result,
                      "Failed to release job for retry due to update job "
                      "status failure. Job id: %s",
                      job_id.c_str());
    release_job_for_retry_context.result = result;
    release_job_for_retry_context.Finish();
    return;
  }

  auto update_visibility_timeout_request =
      make_shared<UpdateJobVisibilityTimeoutRequest>();
  update_visibility_timeout_request->set_job_id(job_id);
  *update_visibility_timeout_request->mutable_duration_to_update() =
      release_job_for_retry_context.request->duration_before_release();
  update_visibility_timeout_request->set_receipt_info(
      job_message_metadata->receipt_info());
  AsyncContext<UpdateJobVisibilityTimeoutRequest,
               UpdateJobVisibilityTimeoutResponse>
      update_job_visibility_timeout_context(
          std::move(update_visibility_timeout_request),
          bind(&JobLifecycleHelper::
                   UpdateJobVisibilityTimeoutCallbackForReleaseJobForRetry,
               this, release_job_for_retry_context, _1),
          release_job_for_retry_context);

  job_client_->UpdateJobVisibilityTimeout(
      update_job_visibility_timeout_context);
}

void JobLifecycleHelper::
    UpdateJobVisibilityTimeoutCallbackForReleaseJobForRetry(
        AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
            release_job_for_retry_context,
        AsyncContext<UpdateJobVisibilityTimeoutRequest,
                     UpdateJobVisibilityTimeoutResponse>&
            update_job_visibility_timeout_context) noexcept {
  const string& job_id = release_job_for_retry_context.request->job_id();
  auto result = update_job_visibility_timeout_context.result;
  if (!result.Successful()) {
    IncrementAggregateMetric(
        job_release_failure_metric_,
        kJobReleaseUpdateJobVisibilityTimeoutFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, release_job_for_retry_context,
                      result,
                      "Failed to release job for retry due to update job "
                      "visibility timeout failure. Job id: %s",
                      job_id.c_str());
    release_job_for_retry_context.result = result;
    release_job_for_retry_context.Finish();
    return;
  }

  result = DeleteJobMetadataFromMap(job_id);
  if (!result.Successful() &&
      result.status_code != SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
    IncrementAggregateMetric(job_metadata_map_failure_metric_,
                             kJobMetadataMapDeleteJobMetadataFailureEventName);
    SCP_ERROR_CONTEXT(kJobLifecycleHelper, release_job_for_retry_context,
                      result,
                      "Failed to remove job message metadata from the "
                      "map. Job id: %s",
                      job_id.c_str());
    release_job_for_retry_context.result = result;
    release_job_for_retry_context.Finish();
    return;
  }

  IncrementAggregateMetric(job_release_metric_, kJobReleaseSuccessEventName);
  release_job_for_retry_context.response =
      make_shared<ReleaseJobForRetryResponse>();
  release_job_for_retry_context.result = SuccessExecutionResult();
  release_job_for_retry_context.Finish();
  return;
}

ExecutionResultOr<ReleaseJobForRetryResponse>
JobLifecycleHelper::ReleaseJobForRetrySync(
    ReleaseJobForRetryRequest request) noexcept {
  ReleaseJobForRetryResponse response;
  auto execution_result = SyncUtils::AsyncToSync2<ReleaseJobForRetryRequest,
                                                  ReleaseJobForRetryResponse>(
      bind(&JobLifecycleHelper::ReleaseJobForRetry, this, _1),
      std::move(request), response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kJobLifecycleHelper, kZeroUuid,
                            "Failed to release job for retry.");
  return response;
}

void JobLifecycleHelper::StartJobExtenderThread() noexcept {
  while (is_running_) {
    vector<string> job_ids;
    RETURN_VOID_IF_FAILURE(job_message_metadata_map_.Keys(job_ids));

    for (string job_id : job_ids) {
      auto job_message_metadata_or = FindJobMetadataById(job_id);
      if (!job_message_metadata_or.Successful()) {
        IncrementAggregateMetric(
            job_metadata_map_failure_metric_,
            kJobMetadataMapFindJobMetadataFailureEventName);
        SCP_ERROR(kJobLifecycleHelper, kZeroUuid,
                  job_message_metadata_or.result(),
                  "Failed to extend job due to finding corresponding job "
                  "message metadata in the map failed. Job id: %s",
                  job_id.c_str());
        continue;
      }

      JobMessageMetadata job_message_metadata = *job_message_metadata_or;
      if (!job_message_metadata.is_visibility_timeout_extendable()) {
        continue;
      }

      const string& receipt_info = job_message_metadata.receipt_info();
      if (receipt_info.empty()) {
        IncrementAggregateMetric(
            job_extender_failure_metric_,
            kJobMetadataMapMissingReceiptInfoFailureEventName);
        SCP_ERROR(
            kJobLifecycleHelper, kZeroUuid,
            FailureExecutionResult(
                SC_JOB_LIFECYCLE_HELPER_MISSING_RECEIPT_INFO),
            "Failed to extend job due to missing receipt info. Job id: %s",
            job_id.c_str());
        auto result = job_message_metadata_map_.Erase(job_id);
        if (!result.Successful()) {
          IncrementAggregateMetric(
              job_metadata_map_failure_metric_,
              kJobMetadataMapDeleteJobMetadataFailureEventName);
          SCP_WARNING(kJobLifecycleHelper, kZeroUuid,
                      "Failed to remove job message metadata from the map. "
                      "Job id: %s",
                      job_id.c_str());
        }
        continue;
      }

      auto get_job_by_id_request = make_shared<GetJobByIdRequest>();
      get_job_by_id_request->set_job_id(job_id);

      auto get_job_by_id_response =
          job_client_->GetJobByIdSync(*get_job_by_id_request);
      if (!get_job_by_id_response.Successful()) {
        IncrementAggregateMetric(job_extender_failure_metric_,

                                 kJobExtenderGetJobByIdFailureEventName);
        SCP_ERROR(
            kJobLifecycleHelper, kZeroUuid, get_job_by_id_response.result(),
            "Failed to extend job due to get job by id failed. Job id: %s",
            job_id.c_str());
        continue;
      }

      if (ExceedingProcessingTimeout(job_id, get_job_by_id_response->job()
                                                 .processing_started_time()
                                                 .seconds())) {
        SCP_INFO(kJobLifecycleHelper, kZeroUuid,
                 "Stop extending job due to exceeding job processing timeout.");
        auto result = DeleteJobMetadataFromMap(job_id);
        if (!result.Successful() &&
            result.status_code != SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
          IncrementAggregateMetric(
              job_metadata_map_failure_metric_,
              kJobMetadataMapDeleteJobMetadataFailureEventName);
          SCP_ERROR(kJobLifecycleHelper, kZeroUuid, result,
                    "Failed to remove job message metadata from the "
                    "map. Job id: %s",
                    job_id.c_str());
        }
        continue;
      }

      auto update_visibility_timeout_request =
          make_shared<UpdateJobVisibilityTimeoutRequest>();
      update_visibility_timeout_request->set_job_id(job_id);
      *update_visibility_timeout_request->mutable_duration_to_update() =
          options_.visibility_timeout_extend_time_seconds();
      update_visibility_timeout_request->set_receipt_info(receipt_info);
      auto update_visibility_timeout_response =
          job_client_->UpdateJobVisibilityTimeoutSync(
              *update_visibility_timeout_request);
      if (!update_visibility_timeout_response.Successful()) {
        IncrementAggregateMetric(
            job_extender_failure_metric_,
            kJobExtenderUpdateJobVisibilityTimeoutFailureEventName);
        SCP_ERROR(kJobLifecycleHelper, kZeroUuid,
                  update_visibility_timeout_response.result(),
                  "Failed to extend job due to update job visibility timeout "
                  "failed. Job id: %s",
                  job_id.c_str());
      }
    }
    sleep(options_.job_extending_worker_sleep_time_seconds().seconds());
  }
}

bool JobLifecycleHelper::ExceedingProcessingTimeout(
    const string& job_id, int64_t processing_started_time_in_seconds) noexcept {
  auto current_time_in_seconds = TimeUtil::GetCurrentTime().seconds();
  auto processing_time_in_seconds =
      current_time_in_seconds - processing_started_time_in_seconds;
  auto timeout = options_.job_processing_timeout_seconds().seconds();
  if (processing_time_in_seconds < timeout) {
    return false;
  }
  SCP_INFO(kJobLifecycleHelper, kZeroUuid,
           "Exceeding job processing timeout. Job id: %s, processing time: %d, "
           "timeout: %d",
           job_id.c_str(), processing_time_in_seconds, timeout);
  return true;
}

ExecutionResultOr<JobMessageMetadata> JobLifecycleHelper::FindJobMetadataById(
    const string& job_id) noexcept {
  JobMessageMetadata job_message_metadata;
  auto result = job_message_metadata_map_.Find(job_id, job_message_metadata);
  if (!result.Successful()) {
    // We don't log the error here if the job is missing in the map. Each API in
    // the JobLifecycleHelper will handle it differently.
    return result;
  }

  const string& receipt_info = job_message_metadata.receipt_info();
  if (receipt_info.empty()) {
    IncrementAggregateMetric(job_metadata_map_failure_metric_,
                             kJobMetadataMapMissingReceiptInfoFailureEventName);
    result =
        FailureExecutionResult(SC_JOB_LIFECYCLE_HELPER_MISSING_RECEIPT_INFO);
    SCP_ERROR(
        kJobLifecycleHelper, kZeroUuid, result,
        "Failed to find corresponding job message metadata due to missing "
        "receipt info. Job id: %s",
        job_id.c_str());
    auto erase_result = job_message_metadata_map_.Erase(job_id);
    if (!erase_result.Successful()) {
      IncrementAggregateMetric(
          job_metadata_map_failure_metric_,
          kJobMetadataMapDeleteJobMetadataFailureEventName);
      SCP_WARNING(kJobLifecycleHelper, kZeroUuid,
                  "Failed to remove job message metadata from the map. "
                  "Job id: %s",
                  job_id.c_str());
    }
    return result;
  }
  return job_message_metadata;
}

ExecutionResult JobLifecycleHelper::DeleteJobMetadataFromMap(
    const string& job_id) noexcept {
  auto result = job_message_metadata_map_.Erase(job_id);
  if (!result.Successful()) {
    if (result.status_code != SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST) {
      SCP_ERROR(kJobLifecycleHelper, kZeroUuid, result,
                "Failed to remove job message metadata from the map due to job "
                "entry does not exist. Job id: %s",
                job_id.c_str());
    } else {
      SCP_WARNING(
          kJobLifecycleHelper, kZeroUuid,
          "Failed to remove job message metadata from the map. Job id: %s",
          job_id.c_str());
    }
  }
  return result;
}

ExecutionResult JobLifecycleHelper::InitJobPreparationMetrics() noexcept {
  job_preparation_metric_labels_ =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kJobLifecycleHelper, kJobPreparationMethodName);

  auto job_preparation_metric_info =
      MetricDefinition(kJobPreparationMetricName, MetricUnit::METRIC_UNIT_COUNT,
                       options_.metric_options().metric_namespace(),
                       job_preparation_metric_labels_);
  job_preparation_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(job_preparation_metric_info),
          {kJobPreparationEventName, kJobPreparationSuccessEventName});
  RETURN_IF_FAILURE(job_preparation_metric_->Init());

  auto job_preparation_failure_metric_info = MetricDefinition(
      kJobPreparationFailureMetricName, MetricUnit::METRIC_UNIT_COUNT,
      options_.metric_options().metric_namespace(),
      job_preparation_metric_labels_);
  job_preparation_failure_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(job_preparation_failure_metric_info),
          {kJobPreparationTryFinishInstanceTerminationFailureEventName,
           kJobPreparationCurrentInstanceTerminationFailureEventName,
           kJobPreparationGetNextJobFailureEventName,
           kJobPreparationUpdateJobStatusFailureEventName});
  RETURN_IF_FAILURE(job_preparation_failure_metric_->Init());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::InitJobCompletionMetrics() noexcept {
  job_completion_metric_labels_ =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kJobLifecycleHelper, kJobCompletionMethodName);

  auto job_completion_metric_info =
      MetricDefinition(kJobCompletionMetricName, MetricUnit::METRIC_UNIT_COUNT,
                       options_.metric_options().metric_namespace(),
                       job_completion_metric_labels_);
  job_completion_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(job_completion_metric_info),
          {kJobCompletionSuccessEventName,
           kJobCompletionJobStatusFailureEventName});
  RETURN_IF_FAILURE(job_completion_metric_->Init());

  auto job_completion_failure_metric_info = MetricDefinition(
      kJobCompletionFailureMetricName, MetricUnit::METRIC_UNIT_COUNT,
      options_.metric_options().metric_namespace(),
      job_completion_metric_labels_);
  job_completion_failure_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(job_completion_failure_metric_info),
          {kJobCompletionInvalidMarkJobCompletedFailureEventName,
           kJobCompletionGetJobByIdFailureEventName,
           kJobCompletionUpdateJobStatusFailureEventName});
  RETURN_IF_FAILURE(job_completion_failure_metric_->Init());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::InitJobReleaseMetrics() noexcept {
  job_release_metric_labels_ =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kJobLifecycleHelper, kJobReleaseMethodName);

  auto job_release_metric_info = MetricDefinition(
      kJobReleaseMetricName, MetricUnit::METRIC_UNIT_COUNT,
      options_.metric_options().metric_namespace(), job_release_metric_labels_);
  job_release_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(job_release_metric_info),
          {kJobReleaseEventName, kJobReleaseSuccessEventName});
  RETURN_IF_FAILURE(job_release_metric_->Init());

  auto job_release_failure_metric_info = MetricDefinition(
      kJobReleaseFailureMetricName, MetricUnit::METRIC_UNIT_COUNT,
      options_.metric_options().metric_namespace(), job_release_metric_labels_);
  job_release_failure_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(job_release_failure_metric_info),
          {kJobReleaseInvalidReleaseJobForRetryFailureEventName,
           kJobReleaseGetJobByIdFailureEventName,
           kJobReleaseInvalidJobStatusFailureEventName,
           kJobReleaseUpdateJobStatusFailureEventName,
           kJobReleaseUpdateJobVisibilityTimeoutFailureEventName});
  RETURN_IF_FAILURE(job_release_failure_metric_->Init());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::InitJobWaitingMetrics() noexcept {
  job_waiting_time_metric_labels_ =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kJobLifecycleHelper, kJobWaitingTimeMethodName);
  auto job_waiting_time_metric_info = MetricDefinition(
      kJobWaitingTimeMetricName, MetricUnit::METRIC_UNIT_MILLISECONDS,
      options_.metric_options().metric_namespace(),
      job_waiting_time_metric_labels_);
  job_waiting_time_metric_ =
      metric_instance_factory_->ConstructSimpleMetricInstance(
          std::move(job_waiting_time_metric_info));
  RETURN_IF_FAILURE(job_waiting_time_metric_->Init());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::InitJobProcessingMetrics() noexcept {
  job_processing_time_metric_labels_ =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kJobLifecycleHelper, kJobProcessingTimeMethodName);
  auto job_processing_time_metric_info = MetricDefinition(
      kJobProcessingTimeMetricName, MetricUnit::METRIC_UNIT_MILLISECONDS,
      options_.metric_options().metric_namespace(),
      job_processing_time_metric_labels_);
  job_processing_time_metric_ =
      metric_instance_factory_->ConstructSimpleMetricInstance(
          std::move(job_processing_time_metric_info));
  RETURN_IF_FAILURE(job_processing_time_metric_->Init());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::InitJobExtenderMetrics() noexcept {
  job_extender_metric_labels_ =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kJobLifecycleHelper, kJobExtenderMethodName);
  auto job_extender_failure_metric_info = MetricDefinition(
      kJobExtenderFailureEventName, MetricUnit::METRIC_UNIT_COUNT,
      options_.metric_options().metric_namespace(),
      job_extender_metric_labels_);
  job_extender_failure_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(job_extender_failure_metric_info),
          {kJobExtenderGetJobByIdFailureEventName,
           kJobExtenderUpdateJobVisibilityTimeoutFailureEventName});
  RETURN_IF_FAILURE(job_extender_failure_metric_->Init());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::InitJobMetadataMapMetrics() noexcept {
  job_metadata_map_metric_labels_ =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kJobLifecycleHelper, kJobMetadataMapMethodName);
  auto job_metadata_map_failure_metric_info = MetricDefinition(
      kJobMetadataMapFailureMetricName, MetricUnit::METRIC_UNIT_COUNT,
      options_.metric_options().metric_namespace(),
      job_metadata_map_metric_labels_);
  job_metadata_map_failure_metric_ =
      metric_instance_factory_->ConstructAggregateMetricInstance(
          std::move(job_metadata_map_failure_metric_info),
          {kJobMetadataMapUpdateJobMetadataFailureEventName,
           kJobMetadataMapDeleteJobMetadataFailureEventName,
           kJobMetadataMapFindJobMetadataFailureEventName,
           kJobMetadataMapInsertJobMetadataFailureEventName,
           kJobMetadataMapMissingReceiptInfoFailureEventName});
  RETURN_IF_FAILURE(job_metadata_map_failure_metric_->Init());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::InitAllMetrics() noexcept {
  if (!metric_instance_factory_) {
    return FailureExecutionResult(
        SC_JOB_LIFECYCLE_HELPER_MISSING_METRIC_INSTANCE_FACTORY);
  }
  RETURN_IF_FAILURE(InitJobPreparationMetrics());
  RETURN_IF_FAILURE(InitJobCompletionMetrics());
  RETURN_IF_FAILURE(InitJobReleaseMetrics());
  RETURN_IF_FAILURE(InitJobWaitingMetrics());
  RETURN_IF_FAILURE(InitJobProcessingMetrics());
  RETURN_IF_FAILURE(InitJobExtenderMetrics());
  RETURN_IF_FAILURE(InitJobMetadataMapMetrics());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::RunAllMetrics() noexcept {
  RETURN_IF_FAILURE(job_preparation_metric_->Run());
  RETURN_IF_FAILURE(job_preparation_failure_metric_->Run());
  RETURN_IF_FAILURE(job_completion_metric_->Run());
  RETURN_IF_FAILURE(job_completion_failure_metric_->Run());
  RETURN_IF_FAILURE(job_release_metric_->Run());
  RETURN_IF_FAILURE(job_release_failure_metric_->Run());
  RETURN_IF_FAILURE(job_waiting_time_metric_->Run());
  RETURN_IF_FAILURE(job_processing_time_metric_->Run());
  RETURN_IF_FAILURE(job_extender_failure_metric_->Run());
  RETURN_IF_FAILURE(job_metadata_map_failure_metric_->Run());
  return SuccessExecutionResult();
}

ExecutionResult JobLifecycleHelper::StopAllMetrics() noexcept {
  RETURN_IF_FAILURE(job_preparation_metric_->Stop());
  RETURN_IF_FAILURE(job_preparation_failure_metric_->Stop());
  RETURN_IF_FAILURE(job_completion_metric_->Stop());
  RETURN_IF_FAILURE(job_completion_failure_metric_->Stop());
  RETURN_IF_FAILURE(job_release_metric_->Stop());
  RETURN_IF_FAILURE(job_release_failure_metric_->Stop());
  RETURN_IF_FAILURE(job_waiting_time_metric_->Stop());
  RETURN_IF_FAILURE(job_processing_time_metric_->Stop());
  RETURN_IF_FAILURE(job_extender_failure_metric_->Stop());
  RETURN_IF_FAILURE(job_metadata_map_failure_metric_->Stop());
  return SuccessExecutionResult();
}

void JobLifecycleHelper::IncrementAggregateMetric(
    shared_ptr<AggregateMetricInterface> metric,
    const string& event_name) noexcept {
  metric->Increment(event_name);
}

void JobLifecycleHelper::RecordTimeInSimpleMetric(
    shared_ptr<SimpleMetricInterface> metric,
    const string& time_in_string) noexcept {
  metric->Push(time_in_string);
}

}  // namespace google::scp::cpio
