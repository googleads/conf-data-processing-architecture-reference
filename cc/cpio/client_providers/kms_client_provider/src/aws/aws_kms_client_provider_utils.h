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

#include <string>

#include <aws/kms/KMSClient.h>

#include "cpio/common/src/aws/error_codes.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Provides utility functions for AWS KMS request flows. Aws uses
 * custom types that need to be converted to SCP types during runtime.
 */
class AwsKmsClientUtils {
 public:
  /**
   * @brief Converts Kms errors to ExecutionResult.
   *
   * @param dynamo_db_error Kms error codes.
   * @return core::ExecutionResult The Kms error code converted to the
   * execution result.
   */
  static core::ExecutionResult ConvertKmsError(
      const Aws::KMS::KMSErrors& kms_error,
      const std::string& error_message) noexcept;
};
}  // namespace google::scp::cpio::client_providers
