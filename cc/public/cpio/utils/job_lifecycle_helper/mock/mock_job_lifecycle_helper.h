/*
 * Copyright 2024 Google LLC
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
#include "public/cpio/utils/job_lifecycle_helper/interface/job_lifecycle_helper_interface.h"

namespace google::scp::cpio {
class MockJobLifecycleHelper : public JobLifecycleHelperInterface {
 public:
  MockJobLifecycleHelper() {
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

  MOCK_METHOD(core::ExecutionResultOr<
                  cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>,
              PrepareNextJobSync,
              (cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest),
              (noexcept, override));

  MOCK_METHOD(
      void, PrepareNextJob,
      ((core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest,
          cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResultOr<
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse>,
      MarkJobCompletedSync,
      (cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest),
      (noexcept, override));

  MOCK_METHOD(
      void, MarkJobCompleted,
      ((core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest,
          cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResultOr<
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>,
      ReleaseJobForRetrySync,
      (cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest),
      (noexcept, override));

  MOCK_METHOD(
      void, ReleaseJobForRetry,
      ((core::AsyncContext<
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryRequest,
          cmrt::sdk::job_lifecycle_helper::v1::ReleaseJobForRetryResponse>)),
      (noexcept, override));
};

}  // namespace google::scp::cpio
