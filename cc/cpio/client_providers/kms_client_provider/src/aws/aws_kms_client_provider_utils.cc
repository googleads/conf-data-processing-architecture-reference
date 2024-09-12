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

#include "aws_kms_client_provider_utils.h"

#include <string>

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/AWSClient.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/kms/KMSClient.h>
#include <aws/kms/KMSErrors.h>
#include <aws/kms/model/DecryptRequest.h>
#include <aws/kms/model/DecryptResult.h>
#include <aws/kms/model/EncryptRequest.h>
#include <aws/kms/model/EncryptResult.h>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "cpio/common/src/aws/error_codes.h"

using Aws::KMS::KMSErrors;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;

namespace {
/// Filename for logging errors
constexpr char kAwsKmsErrorConverter[] = "AwsKmsErrorConverter";
}  // namespace

namespace google::scp::cpio::client_providers {
core::ExecutionResult AwsKmsClientUtils::ConvertKmsError(
    const KMSErrors& kms_error, const std::string& error_message) noexcept {
  auto failure =
      FailureExecutionResult(core::errors::SC_AWS_INTERNAL_SERVICE_ERROR);
  switch (kms_error) {
    case KMSErrors::VALIDATION:
      failure = FailureExecutionResult(core::errors::SC_AWS_VALIDATION_FAILED);
      break;
    case KMSErrors::ACCESS_DENIED:
    case KMSErrors::INVALID_CLIENT_TOKEN_ID:
      failure =
          FailureExecutionResult(core::errors::SC_AWS_INVALID_CREDENTIALS);
      break;
    case KMSErrors::INVALID_PARAMETER_COMBINATION:
    case KMSErrors::INVALID_QUERY_PARAMETER:
    case KMSErrors::INVALID_PARAMETER_VALUE:
    case KMSErrors::MALFORMED_QUERY_STRING:
      failure = FailureExecutionResult(core::errors::SC_AWS_INVALID_REQUEST);
      break;
    case KMSErrors::SERVICE_UNAVAILABLE:
    case KMSErrors::NETWORK_CONNECTION:
      failure =
          FailureExecutionResult(core::errors::SC_AWS_SERVICE_UNAVAILABLE);
      break;
    case KMSErrors::THROTTLING:
      failure =
          FailureExecutionResult(core::errors::SC_AWS_REQUEST_LIMIT_REACHED);
      break;
    case KMSErrors::INVALID_ARN:
      failure = FailureExecutionResult(core::errors::SC_AWS_INVALID_REQUEST);
      break;
    case KMSErrors::INVALID_CIPHERTEXT:
      failure = FailureExecutionResult(core::errors::SC_AWS_INVALID_REQUEST);
      break;
    default:
      failure =
          FailureExecutionResult(core::errors::SC_AWS_INTERNAL_SERVICE_ERROR);
  }

  SCP_ERROR(kAwsKmsErrorConverter, kZeroUuid, failure,
            "AWS cloud service error: code is %d, and error message is %s.",
            kms_error, error_message.c_str());
  return failure;
}
}  // namespace google::scp::cpio::client_providers
