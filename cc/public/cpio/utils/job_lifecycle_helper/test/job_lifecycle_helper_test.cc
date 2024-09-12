// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "public/cpio/utils/job_lifecycle_helper/src/job_lifecycle_helper.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "cc/cpio/client_providers/auto_scaling_client_provider/src/gcp/error_codes.h"
#include "cc/cpio/client_providers/job_client_provider/src/error_codes.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/scp_test_base.h"
#include "google/protobuf/util/time_util.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/mock/auto_scaling_client/mock_auto_scaling_client.h"
#include "public/cpio/mock/job_client/mock_job_client.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"
#include "public/cpio/utils/job_lifecycle_helper/interface/job_lifecycle_helper_interface.h"
#include "public/cpio/utils/job_lifecycle_helper/src/error_codes.h"
#include "public/cpio/utils/metric_instance/mock/mock_metric_instance_factory.h"

using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationResponse;
using google::cmrt::sdk::job_lifecycle_helper::v1::
    JobLifecycleHelperMetricOptions;
using google::cmrt::sdk::job_lifecycle_helper::v1::JobLifecycleHelperOptions;
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
using google::protobuf::Duration;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CPIO_INTERNAL_ERROR;
using google::scp::core::errors::
    SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_GROUP_NAME_REQUIRED;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_UPDATION_CONFLICT;
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
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_ORPHANED_JOB_FOUND;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_RETRY_EXHAUSTED;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::JobLifecycleHelper;
using std::atomic;
using std::atomic_bool;
using std::make_shared;
using std::make_unique;
using std::optional;
using std::shared_ptr;
using std::string;
using std::unique_ptr;

namespace {
constexpr int kRetryLimit = 3;
const Duration kDefaultDurationTime = TimeUtil::SecondsToDuration(0);
const Duration kDefaultVisibilityTimeoutExtendTime =
    TimeUtil::SecondsToDuration(30);
const Duration kCustomDurationBeforeReleaseTime =
    TimeUtil::SecondsToDuration(10);
const Duration kDefaultJobProcessingTimeout = TimeUtil::SecondsToDuration(120);
const Duration kDefaultJobExtendingWorkerSleepTime =
    TimeUtil::SecondsToDuration(1);
constexpr char kQueueMessageReceiptInfo[] = "receipt-info";
constexpr char kJobId[] = "job-id";
constexpr char kServerJobId[] = "server-job-id";
constexpr char kJobBody[] = "jobbody";
constexpr char kScaleInHookName[] = "scale-hook";
constexpr char kMetricNamespace[] = "namespace";

}  // namespace

namespace google::scp::cpio {
class JobLifecycleHelperTest : public ScpTestBase {
 protected:
  void SetUp() override {
    mock_job_client_ = make_unique<MockJobClient>();
    mock_auto_scaling_client_ = make_unique<MockAutoScalingClient>();
    mock_metric_instance_factory_ = make_unique<MockMetricInstanceFactory>();

    options_.set_retry_limit(kRetryLimit);
    *options_.mutable_visibility_timeout_extend_time_seconds() =
        kDefaultVisibilityTimeoutExtendTime;
    *options_.mutable_job_processing_timeout_seconds() =
        kDefaultJobProcessingTimeout;
    *options_.mutable_job_extending_worker_sleep_time_seconds() =
        kDefaultJobExtendingWorkerSleepTime;
    options_.set_current_instance_resource_name(kCurrentInstanceResourceName);
    options_.set_scale_in_hook_name(kScaleInHookName);
    JobLifecycleHelperMetricOptions metric_options;
    metric_options.set_enable_metrics_recording(true);
    *metric_options.mutable_metric_namespace() = kMetricNamespace;
    *options_.mutable_metric_options() = metric_options;
    job_lifecycle_helper_ = make_unique<JobLifecycleHelper>(
        mock_job_client_.get(), mock_auto_scaling_client_.get(),
        mock_metric_instance_factory_.get(), options_);

    prepare_next_job_context_.request = make_shared<PrepareNextJobRequest>();
    prepare_next_job_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    mark_job_completed_context_.request =
        make_shared<MarkJobCompletedRequest>();
    mark_job_completed_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    release_job_for_retry_context_.request =
        make_shared<ReleaseJobForRetryRequest>();
    release_job_for_retry_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    EXPECT_SUCCESS(job_lifecycle_helper_->Init());
    EXPECT_SUCCESS(job_lifecycle_helper_->Run());
  }

  void TearDown() override { EXPECT_SUCCESS(job_lifecycle_helper_->Stop()); }

  void ExpectTryFinishInstanceTermination(
      const ExecutionResult& expected_result,
      const string& expected_instance_resource_name,
      const string& expected_scale_in_hook_name,
      const bool& expected_termination_scheduled) {
    EXPECT_CALL(*mock_auto_scaling_client_, TryFinishInstanceTermination)
        .WillOnce(
            [&expected_result, &expected_instance_resource_name,
             &expected_scale_in_hook_name, &expected_termination_scheduled](
                AsyncContext<TryFinishInstanceTerminationRequest,
                             TryFinishInstanceTerminationResponse>& context) {
              EXPECT_EQ(context.request->instance_resource_name(),
                        expected_instance_resource_name);
              EXPECT_EQ(context.request->scale_in_hook_name(),
                        expected_scale_in_hook_name);
              context.result = expected_result;
              if (expected_result.Successful()) {
                auto response =
                    make_shared<TryFinishInstanceTerminationResponse>();
                response->set_termination_scheduled(
                    expected_termination_scheduled);
                context.response = std::move(response);
              }
              context.Finish();
            });
  }

  void ExpectGetNextJob(const ExecutionResult& expected_result,
                        const Job& expected_job,
                        const string& expected_receipt_info) {
    EXPECT_CALL(*mock_job_client_, GetNextJob)
        .WillOnce(
            [&expected_result, &expected_job, &expected_receipt_info](
                AsyncContext<GetNextJobRequest, GetNextJobResponse>& context) {
              context.result = expected_result;
              if (expected_result.Successful()) {
                auto response = make_shared<GetNextJobResponse>();
                *response->mutable_job() = expected_job;
                response->set_receipt_info(expected_receipt_info);
                context.response = std::move(response);
              }
              context.Finish();
            });
  }

  void ExpectGetJobById(const ExecutionResult& expected_result,
                        const string& job_id, const Job& expected_job) {
    EXPECT_CALL(*mock_job_client_, GetJobById)
        .WillOnce(
            [&expected_result, &job_id, &expected_job](
                AsyncContext<GetJobByIdRequest, GetJobByIdResponse>& context) {
              EXPECT_EQ(context.request->job_id(), job_id);
              context.result = expected_result;
              if (expected_result.Successful()) {
                auto response = make_shared<GetJobByIdResponse>();
                *response->mutable_job() = expected_job;
                context.response = std::move(response);
              }
              context.Finish();
            });
  }

  void ExpectDeleteOrphanedJobMessage(const ExecutionResult& expected_result,
                                      const string& expected_job_id,
                                      const string& expected_receipt_info) {
    EXPECT_CALL(*mock_job_client_, DeleteOrphanedJobMessage)
        .WillOnce([&expected_result, &expected_job_id, &expected_receipt_info](
                      AsyncContext<DeleteOrphanedJobMessageRequest,
                                   DeleteOrphanedJobMessageResponse>& context) {
          context.result = expected_result;
          if (expected_result.Successful()) {
            EXPECT_EQ(context.request->job_id(), expected_job_id);
            EXPECT_EQ(context.request->receipt_info(), expected_receipt_info);
            auto response = make_shared<DeleteOrphanedJobMessageResponse>();
            context.response = std::move(response);
          }
          context.Finish();
        });
  }

  void ExpectUpdateJobStatus(const ExecutionResult& expected_result,
                             const string& expected_job_id,
                             const JobStatus& expected_job_status) {
    EXPECT_CALL(*mock_job_client_, UpdateJobStatus)
        .WillOnce([&expected_result, &expected_job_id, &expected_job_status](
                      AsyncContext<UpdateJobStatusRequest,
                                   UpdateJobStatusResponse>& context) {
          context.result = expected_result;
          if (expected_result.Successful()) {
            EXPECT_EQ(context.request->job_id(), expected_job_id);
            EXPECT_EQ(context.request->job_status(), expected_job_status);
            auto response = make_shared<UpdateJobStatusResponse>();
            context.response = std::move(response);
          }
          context.Finish();
        });
  }

  void ExpectUpdateJobVisibilityTimeout(
      const ExecutionResult& expected_result, const string& expected_job_id,
      const Duration& expected_duration_to_update,
      const string& expected_receipt_info) {
    EXPECT_CALL(*mock_job_client_, UpdateJobVisibilityTimeout)
        .WillOnce(
            [&expected_result, &expected_job_id, &expected_duration_to_update,
             &expected_receipt_info](
                AsyncContext<UpdateJobVisibilityTimeoutRequest,
                             UpdateJobVisibilityTimeoutResponse>& context) {
              context.result = expected_result;
              if (expected_result.Successful()) {
                EXPECT_EQ(context.request->job_id(), expected_job_id);
                EXPECT_EQ(context.request->duration_to_update(),
                          expected_duration_to_update);
                EXPECT_EQ(context.request->receipt_info(),
                          expected_receipt_info);
                auto response =
                    make_shared<UpdateJobVisibilityTimeoutResponse>();
                context.response = std::move(response);
              }
              context.Finish();
            });
  }

  PrepareNextJobRequest prepare_next_job_request_;
  MarkJobCompletedRequest mark_job_completed_request_;

  AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>
      prepare_next_job_context_;
  AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>
      mark_job_completed_context_;
  AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>
      release_job_for_retry_context_;

  JobLifecycleHelperOptions options_;
  unique_ptr<MockJobClient> mock_job_client_;
  unique_ptr<MockAutoScalingClient> mock_auto_scaling_client_;
  unique_ptr<MetricInstanceFactoryInterface> mock_metric_instance_factory_;
  unique_ptr<JobLifecycleHelper> job_lifecycle_helper_;

  const string kCurrentInstanceResourceName =
      R"(//compute.googleapis.com/projects/123456/zones/us-central1-c/instances/1234567)";

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  atomic_bool finish_called_{false};
};

TEST_F(JobLifecycleHelperTest, InitWithNullMetricInstanceFactory) {
  auto client = make_unique<JobLifecycleHelper>(mock_job_client_.get(),
                                                mock_auto_scaling_client_.get(),
                                                nullptr, options_);

  EXPECT_THAT(client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_JOB_LIFECYCLE_HELPER_MISSING_METRIC_INSTANCE_FACTORY)));
}

TEST_F(JobLifecycleHelperTest, InitWithDisabledMetricRecording) {
  JobLifecycleHelperOptions options;
  options.set_retry_limit(kRetryLimit);
  *options.mutable_visibility_timeout_extend_time_seconds() =
      kDefaultVisibilityTimeoutExtendTime;
  *options.mutable_job_processing_timeout_seconds() =
      kDefaultJobProcessingTimeout;
  *options.mutable_job_extending_worker_sleep_time_seconds() =
      kDefaultJobExtendingWorkerSleepTime;
  options.set_current_instance_resource_name(kCurrentInstanceResourceName);
  options.set_scale_in_hook_name(kScaleInHookName);
  JobLifecycleHelperMetricOptions metric_options;
  metric_options.set_enable_metrics_recording(false);
  *metric_options.mutable_metric_namespace() = kMetricNamespace;
  *options.mutable_metric_options() = metric_options;
  auto client = make_unique<JobLifecycleHelper>(mock_job_client_.get(),
                                                mock_auto_scaling_client_.get(),
                                                nullptr, options);

  EXPECT_SUCCESS(client->Init());

  Job orphaned_job;
  orphaned_job.set_job_id(kJobId);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), orphaned_job,
                   kQueueMessageReceiptInfo);
  ExpectDeleteOrphanedJobMessage(SuccessExecutionResult(), kJobId,
                                 kQueueMessageReceiptInfo);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_ORPHANED_JOB_FOUND)));
        finish_called_ = true;
      };

  client->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, PrepareNextJobSuccess) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  *job.mutable_created_time() = TimeUtil::GetCurrentTime();
  string resource_name = "";
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_SUCCESS(prepare_next_job_context.result);
        EXPECT_TRUE(prepare_next_job_context.response != nullptr);
        EXPECT_EQ(prepare_next_job_context.response->job().job_id(), kJobId);
        EXPECT_EQ(prepare_next_job_context.response->job().server_job_id(),
                  kServerJobId);
        EXPECT_EQ(prepare_next_job_context.response->job().job_status(),
                  JobStatus::JOB_STATUS_CREATED);
        EXPECT_EQ(prepare_next_job_context.response->job().job_body(),
                  kJobBody);
        finish_called_ = true;
      };
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest,
       PrepareNextJobWithTryFinishInstanceTerminationFailure) {
  Job job;
  ExpectTryFinishInstanceTermination(
      FailureExecutionResult(
          SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_GROUP_NAME_REQUIRED),
      kCurrentInstanceResourceName, kScaleInHookName, false);
  prepare_next_job_context_.callback = [this](auto& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(
            SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_GROUP_NAME_REQUIRED)));
    finish_called_ = true;
  };
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, PrepareNextJobWithTerminationScheduled) {
  Job job;
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, true);
  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(
            prepare_next_job_context.result,
            ResultIs(FailureExecutionResult(
                SC_JOB_LIFECYCLE_HELPER_CURRENT_INSTANCE_IS_TERMINATING)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, PrepareNextJobWithGetNextJobFailure) {
  Job job;
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM), job,
      kQueueMessageReceiptInfo);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest,
       PrepareNextJobWithOrphanedJobFoundButDeletionFailed) {
  Job orphaned_job;
  orphaned_job.set_job_id(kJobId);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), orphaned_job,
                   kQueueMessageReceiptInfo);
  ExpectDeleteOrphanedJobMessage(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO),
      kJobId, kQueueMessageReceiptInfo);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, PrepareNextJobWithOrphanedJobFoundAndDeleted) {
  Job orphaned_job;
  orphaned_job.set_job_id(kJobId);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), orphaned_job,
                   kQueueMessageReceiptInfo);
  ExpectDeleteOrphanedJobMessage(SuccessExecutionResult(), kJobId,
                                 kQueueMessageReceiptInfo);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_ORPHANED_JOB_FOUND)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, PrepareNextJobWithNextJobAlreadyCompleted) {
  Job job;
  job.set_job_id(kJobId);
  job.set_job_status(JobStatus::JOB_STATUS_FAILURE);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  ExpectDeleteOrphanedJobMessage(SuccessExecutionResult(), kJobId,
                                 kQueueMessageReceiptInfo);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_JOB_ALREADY_COMPLETED)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest,
       PrepareNextJobWithNextJobAlreadyCompletedButDeletionFailed) {
  Job job;
  job.set_job_id(kJobId);
  job.set_job_status(JobStatus::JOB_STATUS_FAILURE);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  ExpectDeleteOrphanedJobMessage(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO),
      kJobId, kQueueMessageReceiptInfo);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, PrepareNextJobWithNextJobAlreadyProcessed) {
  Job job;
  job.set_job_id(kJobId);
  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  *job.mutable_processing_started_time() = TimeUtil::GetCurrentTime();
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_JOB_BEING_PROCESSING)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, PrepareNextJobWithRetryLimitReached) {
  Job job;
  job.set_job_id(kJobId);
  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  job.set_retry_count(4);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  ExpectUpdateJobStatus(SuccessExecutionResult(), kJobId,
                        JobStatus::JOB_STATUS_FAILURE);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_RETRY_EXHAUSTED)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest,
       PrepareNextJobWithRetryLimitReachedButUpdateJobStatusFailure) {
  Job job;
  job.set_job_id(kJobId);
  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  job.set_retry_count(kRetryLimit);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  ExpectUpdateJobStatus(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_UPDATION_CONFLICT), kJobId,
      JobStatus::JOB_STATUS_FAILURE);

  prepare_next_job_context_.callback =
      [this](AsyncContext<PrepareNextJobRequest, PrepareNextJobResponse>&
                 prepare_next_job_context) {
        EXPECT_THAT(prepare_next_job_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_UPDATION_CONFLICT)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, PrepareNextJobSync) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);

  EXPECT_SUCCESS(
      job_lifecycle_helper_->PrepareNextJobSync(prepare_next_job_request_));
}

TEST_F(JobLifecycleHelperTest, MarkJobCompletedSuccess) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  *job.mutable_created_time() = TimeUtil::GetCurrentTime();
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);

  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  ExpectUpdateJobStatus(SuccessExecutionResult(), kJobId,
                        JobStatus::JOB_STATUS_SUCCESS);
  mark_job_completed_context_.request->set_job_id(kJobId);
  mark_job_completed_context_.request->set_job_status(
      JobStatus::JOB_STATUS_SUCCESS);

  mark_job_completed_context_.callback =
      [this](AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>&
                 mark_job_completed_context) {
        EXPECT_SUCCESS(mark_job_completed_context.result);
        EXPECT_TRUE(mark_job_completed_context.response != nullptr);
        finish_called_ = true;
      };
  job_lifecycle_helper_->MarkJobCompleted(mark_job_completed_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, MarkJobCompletedWithMissingJobIdFailure) {
  mark_job_completed_context_.request->set_job_status(
      JobStatus::JOB_STATUS_SUCCESS);
  mark_job_completed_context_.callback =
      [this](AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>&
                 mark_job_completed_context) {
        EXPECT_THAT(mark_job_completed_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_MISSING_JOB_ID)));
        finish_called_ = true;
      };
  job_lifecycle_helper_->MarkJobCompleted(mark_job_completed_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, MarkJobCompletedWithInvalidJobStatusFailure) {
  mark_job_completed_context_.request->set_job_id(kJobId);
  mark_job_completed_context_.request->set_job_status(
      JobStatus::JOB_STATUS_CREATED);
  mark_job_completed_context_.callback =
      [this](AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>&
                 mark_job_completed_context) {
        EXPECT_THAT(mark_job_completed_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_INVALID_JOB_STATUS)));
        finish_called_ = true;
      };
  job_lifecycle_helper_->MarkJobCompleted(mark_job_completed_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, MarkJobCompletedWithGetJobByIdFailure) {
  Job job;
  ExpectGetJobById(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID), kJobId,
      job);
  mark_job_completed_context_.request->set_job_id(kJobId);
  mark_job_completed_context_.request->set_job_status(
      JobStatus::JOB_STATUS_SUCCESS);
  mark_job_completed_context_.callback =
      [this](AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>&
                 mark_job_completed_context) {
        EXPECT_THAT(mark_job_completed_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID)));
        finish_called_ = true;
      };
  job_lifecycle_helper_->MarkJobCompleted(mark_job_completed_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, MarkJobCompletedWithUpdateJobStatusFailure) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);

  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  ExpectUpdateJobStatus(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO),
      kJobId, JobStatus::JOB_STATUS_SUCCESS);
  mark_job_completed_context_.request->set_job_id(kJobId);
  mark_job_completed_context_.request->set_job_status(
      JobStatus::JOB_STATUS_SUCCESS);

  mark_job_completed_context_.callback =
      [this](AsyncContext<MarkJobCompletedRequest, MarkJobCompletedResponse>&
                 mark_job_completed_context) {
        EXPECT_THAT(mark_job_completed_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO)));
        finish_called_ = true;
      };
  job_lifecycle_helper_->MarkJobCompleted(mark_job_completed_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, MarkJobCompletedSync) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  *job.mutable_created_time() = TimeUtil::GetCurrentTime();
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  EXPECT_SUCCESS(
      job_lifecycle_helper_->PrepareNextJobSync(prepare_next_job_request_));

  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  ExpectUpdateJobStatus(SuccessExecutionResult(), kJobId,
                        JobStatus::JOB_STATUS_SUCCESS);
  mark_job_completed_request_.set_job_id(kJobId);
  mark_job_completed_request_.set_job_status(JobStatus::JOB_STATUS_SUCCESS);

  EXPECT_SUCCESS(
      job_lifecycle_helper_->MarkJobCompletedSync(mark_job_completed_request_));
}

TEST_F(JobLifecycleHelperTest, ReleaseJobForRetrySuccess) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);

  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  ExpectUpdateJobStatus(SuccessExecutionResult(), kJobId,
                        JobStatus::JOB_STATUS_CREATED);
  ExpectUpdateJobVisibilityTimeout(SuccessExecutionResult(), kJobId,
                                   kCustomDurationBeforeReleaseTime,
                                   kQueueMessageReceiptInfo);
  release_job_for_retry_context_.request->set_job_id(kJobId);
  *release_job_for_retry_context_.request->mutable_duration_before_release() =
      kCustomDurationBeforeReleaseTime;

  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_SUCCESS(release_job_for_retry_context.result);
        EXPECT_TRUE(release_job_for_retry_context.response != nullptr);
        finish_called_ = true;
      };
  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, ReleaseJobForRetrySuccessWithNoWaitTime) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);

  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  ExpectUpdateJobStatus(SuccessExecutionResult(), kJobId,
                        JobStatus::JOB_STATUS_CREATED);
  ExpectUpdateJobVisibilityTimeout(SuccessExecutionResult(), kJobId,
                                   kDefaultDurationTime,
                                   kQueueMessageReceiptInfo);
  release_job_for_retry_context_.request->set_job_id(kJobId);

  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_SUCCESS(release_job_for_retry_context.result);
        EXPECT_TRUE(release_job_for_retry_context.response != nullptr);
        finish_called_ = true;
      };
  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest,
       ReleaseJobForRetrySuccessWithJobInCreatedStatus) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);

  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  ExpectUpdateJobStatus(SuccessExecutionResult(), kJobId,
                        JobStatus::JOB_STATUS_CREATED);
  ExpectUpdateJobVisibilityTimeout(SuccessExecutionResult(), kJobId,
                                   kCustomDurationBeforeReleaseTime,
                                   kQueueMessageReceiptInfo);
  release_job_for_retry_context_.request->set_job_id(kJobId);
  *release_job_for_retry_context_.request->mutable_duration_before_release() =
      kCustomDurationBeforeReleaseTime;

  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_SUCCESS(release_job_for_retry_context.result);
        EXPECT_TRUE(release_job_for_retry_context.response != nullptr);
        finish_called_ = true;
      };
  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, ReleaseJobForRetryWithMissingJobIdFailure) {
  *release_job_for_retry_context_.request->mutable_duration_before_release() =
      kCustomDurationBeforeReleaseTime;
  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_THAT(release_job_for_retry_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_MISSING_JOB_ID)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest,
       ReleaseJobForRetryWithMissingDurationBeforeReleaseFailure) {
  release_job_for_retry_context_.request->set_job_id(kJobId);
  *release_job_for_retry_context_.request->mutable_duration_before_release() =
      TimeUtil::SecondsToDuration(900);
  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_THAT(
            release_job_for_retry_context.result,
            ResultIs(FailureExecutionResult(
                SC_JOB_LIFECYCLE_HELPER_INVALID_DURATION_BEFORE_RELEASE)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, ReleaseJobForRetryWithGetJobByIdFailure) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);
  ExpectGetJobById(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM), kJobId,
      job);
  release_job_for_retry_context_.request->set_job_id(kJobId);
  *release_job_for_retry_context_.request->mutable_duration_before_release() =
      kCustomDurationBeforeReleaseTime;
  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_THAT(release_job_for_retry_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM)));
        finish_called_ = true;
      };

  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest,
       ReleaseJobForRetryWithInvalidJobStatusForReleaseFailure) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);

  job.set_job_status(JobStatus::JOB_STATUS_FAILURE);
  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  release_job_for_retry_context_.request->set_job_id(kJobId);
  *release_job_for_retry_context_.request->mutable_duration_before_release() =
      kCustomDurationBeforeReleaseTime;

  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_THAT(release_job_for_retry_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_LIFECYCLE_HELPER_INVALID_JOB_STATUS)));
        finish_called_ = true;
      };
  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, ReleaseJobForRetryWithUpdateJobStatusFailure) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);

  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  ExpectUpdateJobStatus(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO),
      kJobId, JobStatus::JOB_STATUS_CREATED);
  release_job_for_retry_context_.request->set_job_id(kJobId);
  *release_job_for_retry_context_.request->mutable_duration_before_release() =
      kCustomDurationBeforeReleaseTime;

  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_THAT(release_job_for_retry_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO)));
        finish_called_ = true;
      };
  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest,
       ReleaseJobForRetryWithUpdateJobVisibilityTimeoutFailure) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  job_lifecycle_helper_->PrepareNextJob(prepare_next_job_context_);

  job.set_job_status(JobStatus::JOB_STATUS_PROCESSING);
  ExpectGetJobById(SuccessExecutionResult(), kJobId, job);
  ExpectUpdateJobStatus(SuccessExecutionResult(), kJobId,
                        JobStatus::JOB_STATUS_CREATED);
  ExpectUpdateJobVisibilityTimeout(
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO),
      kJobId, kCustomDurationBeforeReleaseTime, kQueueMessageReceiptInfo);
  release_job_for_retry_context_.request->set_job_id(kJobId);
  *release_job_for_retry_context_.request->mutable_duration_before_release() =
      kCustomDurationBeforeReleaseTime;

  release_job_for_retry_context_.callback =
      [this](
          AsyncContext<ReleaseJobForRetryRequest, ReleaseJobForRetryResponse>&
              release_job_for_retry_context) {
        EXPECT_THAT(release_job_for_retry_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO)));
        finish_called_ = true;
      };
  job_lifecycle_helper_->ReleaseJobForRetry(release_job_for_retry_context_);
  WaitUntil([&]() { return finish_called_.load(); });
}

TEST_F(JobLifecycleHelperTest, JobExtendSuccess) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  *job.mutable_processing_started_time() = TimeUtil::GetCurrentTime();
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  prepare_next_job_request_.set_is_visibility_timeout_extendable(true);
  EXPECT_SUCCESS(
      job_lifecycle_helper_->PrepareNextJobSync(prepare_next_job_request_));

  GetJobByIdResponse get_job_by_id_response;
  *job.mutable_updated_time() = TimeUtil::SecondsToTimestamp(1672531215);
  *get_job_by_id_response.mutable_job() = std::move(job);

  EXPECT_CALL(*mock_job_client_, GetJobByIdSync)
      .WillOnce([&get_job_by_id_response](GetJobByIdRequest request) {
        EXPECT_EQ(request.job_id(), kJobId);
        return get_job_by_id_response;
      });

  UpdateJobVisibilityTimeoutResponse update_job_visibility_timeout_response;
  EXPECT_CALL(*mock_job_client_, UpdateJobVisibilityTimeoutSync)
      .WillOnce([&update_job_visibility_timeout_response](
                    UpdateJobVisibilityTimeoutRequest request) {
        EXPECT_EQ(request.job_id(), kJobId);
        EXPECT_EQ(request.duration_to_update(),
                  kDefaultVisibilityTimeoutExtendTime);
        EXPECT_EQ(request.receipt_info(), kQueueMessageReceiptInfo);
        return update_job_visibility_timeout_response;
      });

  sleep(kDefaultJobExtendingWorkerSleepTime.seconds());
}

TEST_F(JobLifecycleHelperTest, JobExtendWithVisibilityTimeoutExtendableOff) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  prepare_next_job_request_.set_is_visibility_timeout_extendable(false);
  EXPECT_SUCCESS(
      job_lifecycle_helper_->PrepareNextJobSync(prepare_next_job_request_));

  EXPECT_CALL(*mock_job_client_, GetJobByIdSync).Times(0);
  EXPECT_CALL(*mock_job_client_, UpdateJobVisibilityTimeoutSync).Times(0);

  sleep(kDefaultJobExtendingWorkerSleepTime.seconds());
}

TEST_F(JobLifecycleHelperTest, JobExtendWithMissingReceiptInfo) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, "");
  prepare_next_job_request_.set_is_visibility_timeout_extendable(true);
  EXPECT_SUCCESS(
      job_lifecycle_helper_->PrepareNextJobSync(prepare_next_job_request_));

  EXPECT_CALL(*mock_job_client_, GetJobByIdSync).Times(0);
  EXPECT_CALL(*mock_job_client_, UpdateJobVisibilityTimeoutSync).Times(0);

  sleep(kDefaultJobExtendingWorkerSleepTime.seconds());
}

TEST_F(JobLifecycleHelperTest, JobExtendWithGetJobByIdFailed) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  prepare_next_job_request_.set_is_visibility_timeout_extendable(true);
  EXPECT_SUCCESS(
      job_lifecycle_helper_->PrepareNextJobSync(prepare_next_job_request_));

  EXPECT_CALL(*mock_job_client_, GetJobByIdSync)
      .WillOnce([](GetJobByIdRequest request) {
        return FailureExecutionResult(SC_UNKNOWN);
      });
  EXPECT_CALL(*mock_job_client_, UpdateJobVisibilityTimeoutSync).Times(0);

  sleep(kDefaultJobExtendingWorkerSleepTime.seconds());
}

TEST_F(JobLifecycleHelperTest, JobExtendOverProcessingTimeout) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  *job.mutable_processing_started_time() =
      TimeUtil::SecondsToTimestamp(1704401880);
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  prepare_next_job_request_.set_is_visibility_timeout_extendable(true);
  EXPECT_SUCCESS(
      job_lifecycle_helper_->PrepareNextJobSync(prepare_next_job_request_));

  GetJobByIdResponse get_job_by_id_response;
  *get_job_by_id_response.mutable_job() = std::move(job);
  EXPECT_CALL(*mock_job_client_, GetJobByIdSync)
      .WillOnce([&get_job_by_id_response](GetJobByIdRequest request) {
        return get_job_by_id_response;
      });
  EXPECT_CALL(*mock_job_client_, UpdateJobVisibilityTimeoutSync).Times(0);

  sleep(kDefaultJobExtendingWorkerSleepTime.seconds());
}

TEST_F(JobLifecycleHelperTest, JobExtendWithUpdateVisibilityTimeoutFailed) {
  Job job;
  job.set_job_id(kJobId);
  job.set_server_job_id(kServerJobId);
  job.set_job_status(JobStatus::JOB_STATUS_CREATED);
  job.set_job_body(kJobBody);
  *job.mutable_processing_started_time() = TimeUtil::GetCurrentTime();
  ExpectTryFinishInstanceTermination(SuccessExecutionResult(),
                                     kCurrentInstanceResourceName,
                                     kScaleInHookName, false);
  ExpectGetNextJob(SuccessExecutionResult(), job, kQueueMessageReceiptInfo);
  prepare_next_job_request_.set_is_visibility_timeout_extendable(true);
  EXPECT_SUCCESS(
      job_lifecycle_helper_->PrepareNextJobSync(prepare_next_job_request_));

  GetJobByIdResponse get_job_by_id_response;
  *get_job_by_id_response.mutable_job() = std::move(job);
  EXPECT_CALL(*mock_job_client_, GetJobByIdSync)
      .WillOnce([&get_job_by_id_response](GetJobByIdRequest request) {
        return get_job_by_id_response;
      });
  EXPECT_CALL(*mock_job_client_, UpdateJobVisibilityTimeoutSync)
      .WillOnce([](UpdateJobVisibilityTimeoutRequest request) {
        return FailureExecutionResult(SC_UNKNOWN);
      });

  sleep(kDefaultJobExtendingWorkerSleepTime.seconds());
}

}  // namespace google::scp::cpio
