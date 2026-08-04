// Copyright 2022 Google LLC
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

#include "core/utils/src/error_utils.h"

#include <gtest/gtest.h>

#include "core/interface/errors.h"
#include "core/interface/type_def.h"
#include "core/utils/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/error_codes.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::ResultIs;

namespace google::scp::core::utils::test {
TEST(ErrorUtilsTest, ConvertSuccessExecutionResult) {
  EXPECT_SUCCESS(ConvertToPublicExecutionResult(SuccessExecutionResult()));
}

TEST(ErrorUtilsTest, ConvertFailureExecutionResult) {
  FailureExecutionResult failure(SC_UNKNOWN);
  EXPECT_THAT(ConvertToPublicExecutionResult(failure),
              ResultIs(FailureExecutionResult(errors::SC_CPIO_UNKNOWN_ERROR)));
}

TEST(ErrorUtilsTest, ConvertRetryExecutionResult) {
  RetryExecutionResult retry(SC_UNKNOWN);
  EXPECT_THAT(ConvertToPublicExecutionResult(retry),
              ResultIs(RetryExecutionResult(errors::SC_CPIO_UNKNOWN_ERROR)));
}

TEST(ErrorUtilsTest, ConvertUnmappedExecutionResult) {
  uint64_t unmapped_error = 0x12345678;
  FailureExecutionResult failure(unmapped_error);
  EXPECT_THAT(ConvertToPublicExecutionResult(failure), ResultIs(failure));
}

TEST(ErrorUtilsTest, ConvertCoreUtilsErrorCodes) {
  FailureExecutionResult invalid_input(errors::SC_CORE_UTILS_INVALID_INPUT);
  EXPECT_THAT(
      ConvertToPublicExecutionResult(invalid_input),
      ResultIs(FailureExecutionResult(errors::SC_CPIO_INVALID_ARGUMENT)));

  FailureExecutionResult invalid_base64(
      errors::SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH);
  EXPECT_THAT(
      ConvertToPublicExecutionResult(invalid_base64),
      ResultIs(FailureExecutionResult(errors::SC_CPIO_INVALID_ARGUMENT)));

  FailureExecutionResult curl_init_error(errors::SC_CORE_UTILS_CURL_INIT_ERROR);
  EXPECT_THAT(ConvertToPublicExecutionResult(curl_init_error),
              ResultIs(FailureExecutionResult(errors::SC_CPIO_INTERNAL_ERROR)));
}
}  // namespace google::scp::core::utils::test
