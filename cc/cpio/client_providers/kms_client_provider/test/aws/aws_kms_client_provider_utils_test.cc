// Copyright 2024 Google LLC
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

#include "cpio/client_providers/kms_client_provider/src/aws/aws_kms_client_provider_utils.h"

#include <string>

#include <aws/core/Aws.h>
#include <aws/kms/KMSErrors.h>

#include "core/test/utils/scp_test_base.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::Client::AWSError;
using Aws::KMS::KMSErrors;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using std::get;
using std::make_tuple;
using std::string;
using std::tuple;
using testing::Values;
using testing::WithParamInterface;

namespace google::scp::cpio::client_providers::test {
class KMSErrorConverterTest
    : public ScpTestBase,
      public WithParamInterface<tuple<KMSErrors, FailureExecutionResult>> {
 protected:
  KMSErrors GetKMSError() { return get<0>(GetParam()); }

  FailureExecutionResult GetExpectedFailureExecutionResult() {
    return get<1>(GetParam());
  }
};

TEST_P(KMSErrorConverterTest, KMSErrorConverter) {
  auto error_code = GetKMSError();
  EXPECT_THAT(AwsKmsClientUtils::ConvertKmsError(error_code, "failure"),
              ResultIs(GetExpectedFailureExecutionResult()));
}

INSTANTIATE_TEST_SUITE_P(
    KMSErrorConverterTest, KMSErrorConverterTest,
    Values(make_tuple(KMSErrors::VALIDATION,
                      FailureExecutionResult(SC_AWS_VALIDATION_FAILED)),
           make_tuple(KMSErrors::ACCESS_DENIED,
                      FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)),
           make_tuple(KMSErrors::INVALID_CLIENT_TOKEN_ID,
                      FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)),
           make_tuple(KMSErrors::INVALID_PARAMETER_COMBINATION,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(KMSErrors::INVALID_QUERY_PARAMETER,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(KMSErrors::INVALID_PARAMETER_VALUE,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(KMSErrors::MALFORMED_QUERY_STRING,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(KMSErrors::SERVICE_UNAVAILABLE,
                      FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)),
           make_tuple(KMSErrors::NETWORK_CONNECTION,
                      FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)),
           make_tuple(KMSErrors::THROTTLING,
                      FailureExecutionResult(SC_AWS_REQUEST_LIMIT_REACHED)),
           make_tuple(KMSErrors::INTERNAL_FAILURE,
                      FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)),
           make_tuple(KMSErrors::INVALID_ARN,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST)),
           make_tuple(KMSErrors::INVALID_CIPHERTEXT,
                      FailureExecutionResult(SC_AWS_INVALID_REQUEST))));

}  // namespace google::scp::cpio::client_providers::test
