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

#include <gmock/gmock.h>

#include <memory>

#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/job_client/job_client_interface.h"

namespace google::scp::cpio {
class MockJobClient : public JobClientInterface {
 public:
  MockJobClient() {
    ON_CALL(*this, Init)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
  }

  MOCK_METHOD(core::ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Run, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(
      void, PutJob,
      ((core::AsyncContext<cmrt::sdk::job_service::v1::PutJobRequest,
                           cmrt::sdk::job_service::v1::PutJobResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResultOr<cmrt::sdk::job_service::v1::PutJobResponse>,
      PutJobSync, (cmrt::sdk::job_service::v1::PutJobRequest),
      (noexcept, override));

  MOCK_METHOD(
      void, GetNextJob,
      ((core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                           cmrt::sdk::job_service::v1::GetNextJobResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResultOr<cmrt::sdk::job_service::v1::GetNextJobResponse>,
      GetNextJobSync, (cmrt::sdk::job_service::v1::GetNextJobRequest),
      (noexcept, override));

  MOCK_METHOD(
      void, GetJobById,
      ((core::AsyncContext<cmrt::sdk::job_service::v1::GetJobByIdRequest,
                           cmrt::sdk::job_service::v1::GetJobByIdResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResultOr<cmrt::sdk::job_service::v1::GetJobByIdResponse>,
      GetJobByIdSync, (cmrt::sdk::job_service::v1::GetJobByIdRequest),
      (noexcept, override));

  MOCK_METHOD(void, UpdateJobBody,
              ((core::AsyncContext<
                  cmrt::sdk::job_service::v1::UpdateJobBodyRequest,
                  cmrt::sdk::job_service::v1::UpdateJobBodyResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<
                  cmrt::sdk::job_service::v1::UpdateJobBodyResponse>,
              UpdateJobBodySync,
              (cmrt::sdk::job_service::v1::UpdateJobBodyRequest),
              (noexcept, override));

  MOCK_METHOD(void, UpdateJobStatus,
              ((core::AsyncContext<
                  cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                  cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<
                  cmrt::sdk::job_service::v1::UpdateJobStatusResponse>,
              UpdateJobStatusSync,
              (cmrt::sdk::job_service::v1::UpdateJobStatusRequest),
              (noexcept, override));

  MOCK_METHOD(
      void, UpdateJobVisibilityTimeout,
      ((core::AsyncContext<
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest,
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResultOr<
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>,
      UpdateJobVisibilityTimeoutSync,
      (cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest),
      (noexcept, override));

  MOCK_METHOD(
      void, DeleteOrphanedJobMessage,
      ((core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&)),
      (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<
                  cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>,
              DeleteOrphanedJobMessageSync,
              (cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest),
              (noexcept, override));
};

}  // namespace google::scp::cpio
