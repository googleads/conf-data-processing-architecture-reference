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
#include "public/cpio/interface/queue_client/queue_client_interface.h"

namespace google::scp::cpio {
class MockQueueClient : public QueueClientInterface {
 public:
  MockQueueClient() {
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

  MOCK_METHOD(void, EnqueueMessage,
              ((core::AsyncContext<
                  cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                  cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<
                  cmrt::sdk::queue_service::v1::EnqueueMessageResponse>,
              EnqueueMessageSync,
              ((cmrt::sdk::queue_service::v1::EnqueueMessageRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetTopMessage,
              ((core::AsyncContext<
                  cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                  cmrt::sdk::queue_service::v1::GetTopMessageResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<
                  cmrt::sdk::queue_service::v1::GetTopMessageResponse>,
              GetTopMessageSync,
              ((cmrt::sdk::queue_service::v1::GetTopMessageRequest)),
              (noexcept, override));

  MOCK_METHOD(
      void, UpdateMessageVisibilityTimeout,
      ((core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::
              UpdateMessageVisibilityTimeoutResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResultOr<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>,
      UpdateMessageVisibilityTimeoutSync,
      ((cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest
            request)),
      (noexcept, override));

  MOCK_METHOD(void, DeleteMessage,
              ((core::AsyncContext<
                  cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                  cmrt::sdk::queue_service::v1::DeleteMessageResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<
                  cmrt::sdk::queue_service::v1::DeleteMessageResponse>,
              DeleteMessageSync,
              ((cmrt::sdk::queue_service::v1::DeleteMessageRequest request)),
              (noexcept, override));
};
}  // namespace google::scp::cpio
