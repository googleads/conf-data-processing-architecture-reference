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

#ifndef SCP_CPIO_INTERFACE_JOB_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_JOB_CLIENT_INTERFACE_H_

#include <memory>
#include <string>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {

/**
 * @brief Interface responsible for storing and fetching jobs.
 *
 * Use JobClientFactory::Create to create the JobClient. Call
 * JobClientInterface::Init and JobClientInterface::Run before actually using
 * it, and call JobClientInterface::Stop when finished using it.
 *
 */
class JobClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Put a Job.
   *
   * @param put_job_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual void PutJob(
      core::AsyncContext<cmrt::sdk::job_service::v1::PutJobRequest,
                         cmrt::sdk::job_service::v1::PutJobResponse>&
          put_job_context) noexcept = 0;

  /**
   * @brief Put a Job in a blocking call.
   *
   * @param request request to put job.
   * @return ExecutionResultOr<PutJobResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<cmrt::sdk::job_service::v1::PutJobResponse>
  PutJobSync(cmrt::sdk::job_service::v1::PutJobRequest request) noexcept = 0;

  /**
   * @brief Get the first available Job.
   *
   * @param get_next_job_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual void GetNextJob(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                         cmrt::sdk::job_service::v1::GetNextJobResponse>&
          get_next_job_context) noexcept = 0;

  /**
   * @brief Get the first available Job in a blocking call.
   *
   * @param request request to get next job.
   * @return ExecutionResultOr<GetNextJobResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_service::v1::GetNextJobResponse>
  GetNextJobSync(
      cmrt::sdk::job_service::v1::GetNextJobRequest request) noexcept = 0;

  /**
   * @brief Get a Job by job id.
   *
   * @param get_job_by_id_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual void GetJobById(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetJobByIdRequest,
                         cmrt::sdk::job_service::v1::GetJobByIdResponse>&
          get_job_by_id_context) noexcept = 0;

  /**
   * @brief Get a Job by job id in a blocking call.
   *
   * @param request request to get job by ID.
   * @return ExecutionResultOr<GetJobByIdResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_service::v1::GetJobByIdResponse>
  GetJobByIdSync(
      cmrt::sdk::job_service::v1::GetJobByIdRequest request) noexcept = 0;

  /**
   * @brief Update job body of a Job.
   *
   * @param update_job_body_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual void UpdateJobBody(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobBodyRequest,
                         cmrt::sdk::job_service::v1::UpdateJobBodyResponse>&
          update_job_body_context) noexcept = 0;

  /**
   * @brief Update job body of a Job in a blocking call.
   *
   * @param request request to update job body.
   * @return ExecutionResultOr<UpdateJobBodyResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_service::v1::UpdateJobBodyResponse>
  UpdateJobBodySync(
      cmrt::sdk::job_service::v1::UpdateJobBodyRequest request) noexcept = 0;

  /**
   * @brief Update status of a Job.
   *
   * @param update_job_status_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual void UpdateJobStatus(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context) noexcept = 0;

  /**
   * @brief Update status of a Job in a blocking call.
   *
   * @param request request to update status.
   * @return ExecutionResultOr<UpdateJobStatusResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_service::v1::UpdateJobStatusResponse>
  UpdateJobStatusSync(
      cmrt::sdk::job_service::v1::UpdateJobStatusRequest request) noexcept = 0;

  /**
   * @brief Update visibility timeout of a Job.
   *
   * @param update_job_visibility_timeout_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual void UpdateJobVisibilityTimeout(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest,
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>&
          update_job_visibility_timeout_context) noexcept = 0;

  /**
   * @brief Update visibility timeout of a Job in a blocking call.
   *
   * @param request request to update job visibility timeout.
   * @return ExecutionResultOr<UpdateJobVisibilityTimeoutResponse> result of the
   * operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>
  UpdateJobVisibilityTimeoutSync(
      cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest
          request) noexcept = 0;

  /**
   * @brief Deletes the orphaned job from the job queue.
   *
   * @param delete_orphaned_job_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual void DeleteOrphanedJobMessage(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&
          delete_orphaned_job_context) noexcept = 0;

  /**
   * @brief Deletes the orphaned job from the job queue in a blocking call.
   *
   * @param request request to delete orphaned job message.
   * @return ExecutionResultOr<DeleteOrphanedJobMessageResponse> result of the
   * operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>
  DeleteOrphanedJobMessageSync(
      cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest
          request) noexcept = 0;
};

// Factory to create JobClient.
class JobClientFactory {
 public:
  /**
   * @brief Creates JobClient.
   *
   * @param options JobClientOptions.
   * @return std::shared_ptr<JobClientInterface> created JobClient object.
   */
  static std::unique_ptr<JobClientInterface> Create(
      JobClientOptions options = JobClientOptions()) noexcept;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_JOB_CLIENT_INTERFACE_H_
