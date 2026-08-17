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

#include "cpio/client_providers/role_credentials_provider/src/aws/sts_error_converter.h"

#include <gtest/gtest.h>

#include <aws/core/client/AWSError.h>
#include <aws/sts/STSErrors.h>

#include "cpio/common/src/aws/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::Client::AWSError;
using Aws::STS::STSErrors;
using google::scp::core::FailureExecutionResult;

using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using google::scp::core::test::ResultIs;

namespace google::scp::cpio::client_providers::test {
TEST(STSErrorConverter, SucceededToConvertHandledSTSErrors) {
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  AWSError<STSErrors>(STSErrors::VALIDATION, false)),
              ResultIs(FailureExecutionResult(SC_AWS_VALIDATION_FAILED)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  AWSError<STSErrors>(STSErrors::ACCESS_DENIED, false)),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(AWSError<STSErrors>(
                  STSErrors::INVALID_PARAMETER_COMBINATION, false)),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(AWSError<STSErrors>(
                  STSErrors::INVALID_QUERY_PARAMETER, false)),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(AWSError<STSErrors>(
                  STSErrors::INVALID_PARAMETER_VALUE, false)),
              ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  AWSError<STSErrors>(STSErrors::INTERNAL_FAILURE, false)),
              ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  AWSError<STSErrors>(STSErrors::SERVICE_UNAVAILABLE, false)),
              ResultIs(FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  AWSError<STSErrors>(STSErrors::NETWORK_CONNECTION, false)),
              ResultIs(FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)));
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(
                  AWSError<STSErrors>(STSErrors::THROTTLING, false)),
              ResultIs(FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED)));
}

TEST(STSErrorConverter, SucceededToConvertNonHandledSTSErrors) {
  EXPECT_THAT(STSErrorConverter::ConvertSTSError(AWSError<STSErrors>(
                  STSErrors::MALFORMED_QUERY_STRING, false)),
              ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
}
}  // namespace google::scp::cpio::client_providers::test
