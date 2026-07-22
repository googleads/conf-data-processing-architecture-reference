/*
 * Copyright 2026 Google LLC
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
#include <string>
#include <vector>

#include "public/core/interface/execution_result.h"

#include "wrapped_key_handler_with_cache_interface.h"

namespace google::scp::cpio {

class WrappedKeyHandlerWithCacheMock
    : public WrappedKeyHandlerWithCacheInterface {
 public:
  WrappedKeyHandlerWithCacheMock() {
    ON_CALL(*this, Init)
        .WillByDefault(
            testing::Return(google::scp::core::SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(
            testing::Return(google::scp::core::SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(
            testing::Return(google::scp::core::SuccessExecutionResult()));
  }

  MOCK_METHOD(google::scp::core::ExecutionResult, Init, (),
              (noexcept, override));
  MOCK_METHOD(google::scp::core::ExecutionResult, Run, (),
              (noexcept, override));
  MOCK_METHOD(google::scp::core::ExecutionResult, Stop, (),
              (noexcept, override));

  MOCK_METHOD(google::scp::core::ExecutionResultOr<std::string>, GetKey,
              (const google::cmrt::sdk::v1::CloudWrappedKey&),
              (noexcept, override));
};

}  // namespace google::scp::cpio
