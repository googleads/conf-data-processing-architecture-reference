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
#pragma once

#include <string>

#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/utils/job_lifecycle_helper/proto/v1/job_lifecycle_helper.pb.h"

namespace google::scp::cpio {
/**
 * @brief Helper to cleans dangling job messages, extends job visibility timeout
 * and also validates job status before jobs get processed.
 */
class JobLifecycleHelperInterface : public core::ServiceInterface {
 public:
  virtual ~JobLifecycleHelperInterface() = default;

  /**
   * @brief Prepare the next available job and make it ready to be processed.
   *
   * @param request the prepare job request.
   * @return core::ExecutionResultOr<PrepareNextJobResponse> the preparae next
   * job response.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>
  PrepareNextJobSync(cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest
                         request) noexcept = 0;

  /**
   * @brief Prepare the next available job and make it ready to be processed.
   *
   * @param prepare_next_job_context the async context for the operation.
   * @return core::ExecutionResult the job preparation result.
   */
  virtual void PrepareNextJob(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>
          prepare_next_job_context) noexcept = 0;

  /**
   * @brief Mark the job completed with Success or Failure state.
   *
   * @param request the mark job completed request.
   * @return core::ExecutionResult the mark job completed response.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse>
  MarkJobCompletedSync(
      cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest
          request) noexcept = 0;

  /**
   * @brief Mark the job completed with Success or Failure state.
   *
   * @param mark_job_completed_context the async context for the operation.
   * @return core::ExecutionResult the mark job completed result.
   */
  virtual void MarkJobCompleted(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest,
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse>
          mark_job_completed_context) noexcept = 0;

  /**
   * @brief Release a job from current worker to the job queue for retry.
   *
   * @param request the release job for retry request.
   * @return core::ExecutionResultOr<ReleaseJobForRetryResponse> the release job
   * for retry response.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>
  ReleaseJobForRetrySync(
      cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest
          request) noexcept = 0;

  /**
   * @brief Release a job from current worker to the job queue for retry.
   *
   * @param release_job_for_retry_context the async context for the operation.
   * @return core::ExecutionResult the release job for retry result.
   */
  virtual void ReleaseJobForRetry(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest,
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>
          release_job_for_retry_context) noexcept = 0;
};

}  // namespace google::scp::cpio
