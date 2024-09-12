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

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/auto_scaling_client/auto_scaling_client_interface.h"
#include "public/cpio/interface/job_client/job_client_interface.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"
#include "public/cpio/utils/job_lifecycle_helper/interface/job_lifecycle_helper_interface.h"
#include "public/cpio/utils/job_lifecycle_helper/proto/v1/job_lifecycle_helper.pb.h"
#include "public/cpio/utils/metric_instance/interface/metric_instance_factory_interface.h"
#include "public/cpio/utils/metric_instance/interface/simple_metric_interface.h"
#include "public/cpio/utils/metric_instance/noop/noop_metric_instance_factory.h"

#include "error_codes.h"

namespace google::scp::cpio {
/*! @copydoc JobLifecycleHelperInterface
 */
class JobLifecycleHelper : public JobLifecycleHelperInterface {
 public:
  /**
   * @brief Construct a new Job Lifecycle Helper object.
   *
   * @param job_client the jobClient object.
   * @param options the options of Job Lifecycle Helper.
   */
  explicit JobLifecycleHelper(
      JobClientInterface* job_client,
      AutoScalingClientInterface* auto_scaling_client,
      MetricInstanceFactoryInterface* metric_instance_factory,
      cmrt::sdk::job_lifecycle_helper::v1::JobLifecycleHelperOptions options)
      : job_client_(job_client),
        auto_scaling_client_(auto_scaling_client),
        metric_instance_factory_(
            options.metric_options().enable_metrics_recording()
                ? metric_instance_factory
                : new NoopMetricInstanceFactory()),
        options_(std::move(options)),
        is_running_(false) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>
  PrepareNextJobSync(cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest
                         request) noexcept override;

  void PrepareNextJob(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>
          prepare_job_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse>
  MarkJobCompletedSync(
      cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest
          request) noexcept override;

  void MarkJobCompleted(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest,
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse>
          mark_job_completed_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>
  ReleaseJobForRetrySync(
      cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest
          request) noexcept override;

  void ReleaseJobForRetry(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest,
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>
          release_job_for_retry_context) noexcept override;

 private:
  void TryFinishInstanceTerminationCallback(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>&
          prepare_job_context,
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>&
          try_finish_instance_termination_context) noexcept;

  void GetNextJobCallback(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>&
          prepare_job_context,
      core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                         cmrt::sdk::job_service::v1::GetNextJobResponse>&
          get_next_job_context) noexcept;

  void DeleteOrphanedJobMessage(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>&
          prepare_job_context,
      core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                         cmrt::sdk::job_service::v1::GetNextJobResponse>&
          get_next_job_context) noexcept;

  void DeleteOrphanedJobMessageCallback(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>&
          prepare_job_context,
      core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&
          delete_orphaned_job_message_context) noexcept;

  core::ExecutionResult InsertJobMessageMetadataToMap(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>&
          prepare_job_context,
      cmrt::sdk::job_lifecycle_helper::v1::JobMessageMetadata
          updated_job_message_metadata) noexcept;

  void UpdateJobStatusCallbackForPrepareJob(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>&
          prepare_job_context,
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context) noexcept;

  void GetJobByIdCallbackForMarkJobCompleted(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest,
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse>&
          mark_job_completed_context,
      core::AsyncContext<cmrt::sdk::job_service::v1::GetJobByIdRequest,
                         cmrt::sdk::job_service::v1::GetJobByIdResponse>&
          get_job_by_id_context) noexcept;

  void UpdateJobStatusCallbackForMarkJobCompleted(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest,
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse>&
          mark_job_completed_context,
      std::shared_ptr<google::protobuf::Timestamp> processing_started_time,
      std::shared_ptr<google::protobuf::Timestamp> created_time,
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context) noexcept;

  void GetJobByIdCallbackForReleaseJobForRetry(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest,
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>&
          release_job_for_retry_context,
      std::shared_ptr<cmrt::sdk::job_lifecycle_helper::v1::JobMessageMetadata>
          job_message_metadata,
      core::AsyncContext<cmrt::sdk::job_service::v1::GetJobByIdRequest,
                         cmrt::sdk::job_service::v1::GetJobByIdResponse>&
          get_job_by_id_context) noexcept;

  void UpdateJobStatusCallbackForReleaseJobForRetry(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest,
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>&
          release_job_for_retry_context,
      std::shared_ptr<cmrt::sdk::job_lifecycle_helper::v1::JobMessageMetadata>
          job_message_metadata,
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context) noexcept;

  void UpdateJobVisibilityTimeoutCallbackForReleaseJobForRetry(
      core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest,
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>&
          release_job_for_retry_context,
      core::AsyncContext<
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest,
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>&
          update_job_visibility_timeout_context) noexcept;

  void StartJobExtenderThread() noexcept;

  core::ExecutionResult InitJobPreparationMetrics() noexcept;

  core::ExecutionResult InitJobCompletionMetrics() noexcept;

  core::ExecutionResult InitJobReleaseMetrics() noexcept;

  core::ExecutionResult InitJobWaitingMetrics() noexcept;

  core::ExecutionResult InitJobProcessingMetrics() noexcept;

  core::ExecutionResult InitJobExtenderMetrics() noexcept;

  core::ExecutionResult InitJobMetadataMapMetrics() noexcept;

  core::ExecutionResult InitAllMetrics() noexcept;

  core::ExecutionResult RunAllMetrics() noexcept;

  core::ExecutionResult StopAllMetrics() noexcept;

  bool ExceedingProcessingTimeout(
      const std::string& job_id,
      int64_t processing_started_time_in_seconds) noexcept;

  core::ExecutionResultOr<
      cmrt::sdk::job_lifecycle_helper::v1::JobMessageMetadata>
  FindJobMetadataById(const std::string& job_id) noexcept;

  core::ExecutionResult DeleteJobMetadataFromMap(
      const std::string& job_id) noexcept;

  void IncrementAggregateMetric(
      std::shared_ptr<cpio::AggregateMetricInterface> metric,
      const std::string& event_name) noexcept;

  void RecordTimeInSimpleMetric(
      std::shared_ptr<cpio::SimpleMetricInterface> metric,
      const std::string& time_in_string) noexcept;

  // The job client.
  JobClientInterface* job_client_;

  // The auto scaling client.
  AutoScalingClientInterface* auto_scaling_client_;

  // The metric instance factory.
  MetricInstanceFactoryInterface* metric_instance_factory_;

  // The metrics labels for job preparation.
  std::map<std::string, std::string> job_preparation_metric_labels_;

  /// The aggregate metric instance for job preparation count.
  std::shared_ptr<cpio::AggregateMetricInterface> job_preparation_metric_;

  /// The aggregate metric instance for job preparation failure.
  std::shared_ptr<cpio::AggregateMetricInterface>
      job_preparation_failure_metric_;

  // The metrics labels for job completion.
  std::map<std::string, std::string> job_completion_metric_labels_;

  /// The aggregate metric instance for job completion count.
  std::shared_ptr<cpio::AggregateMetricInterface> job_completion_metric_;

  /// The aggregate metric instance for job completion failure.
  std::shared_ptr<cpio::AggregateMetricInterface>
      job_completion_failure_metric_;

  // The metrics labels for job release.
  std::map<std::string, std::string> job_release_metric_labels_;

  /// The aggregate metric instance for job release count.
  std::shared_ptr<cpio::AggregateMetricInterface> job_release_metric_;

  /// The aggregate metric instance for job release failure.
  std::shared_ptr<cpio::AggregateMetricInterface> job_release_failure_metric_;

  // The metrics labels for job waiting time.
  std::map<std::string, std::string> job_waiting_time_metric_labels_;

  /// The aggregate metric instance for job waiting time.
  std::shared_ptr<cpio::SimpleMetricInterface> job_waiting_time_metric_;

  // The metrics labels for job processing time.
  std::map<std::string, std::string> job_processing_time_metric_labels_;

  /// The aggregate metric instance for job processing time.
  std::shared_ptr<cpio::SimpleMetricInterface> job_processing_time_metric_;

  // The metrics labels for job extender.
  std::map<std::string, std::string> job_extender_metric_labels_;

  /// The aggregate metric instance for job extender failure.
  std::shared_ptr<cpio::AggregateMetricInterface> job_extender_failure_metric_;

  // The metrics labels for job metadata map.
  std::map<std::string, std::string> job_metadata_map_metric_labels_;

  /// The aggregate metric instance for job metadata map operations failure.
  std::shared_ptr<cpio::AggregateMetricInterface>
      job_metadata_map_failure_metric_;

  // The options of Job Lifecycle helper.
  cmrt::sdk::job_lifecycle_helper::v1::JobLifecycleHelperOptions options_;

  // The job extender worker thread.
  std::unique_ptr<std::thread> job_extender_worker_;

  std::atomic<bool> is_running_;

  // The cache to hold metadata of job messages.
  // The key is job id.
  core::common::ConcurrentMap<
      std::string, cmrt::sdk::job_lifecycle_helper::v1::JobMessageMetadata>
      job_message_metadata_map_;
};
}  // namespace google::scp::cpio
