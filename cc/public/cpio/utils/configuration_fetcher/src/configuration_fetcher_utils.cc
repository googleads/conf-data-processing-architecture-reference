/*
 * Copyright 2023 Google LLC
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

#include "configuration_fetcher_utils.h"

#include <sstream>
#include <string>

#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using std::string;

namespace google::scp::cpio {
ExecutionResultOr<bool> ConfigurationFetcherUtils::StringToBool(
    const string& value) {
  if (value == "true" || value == "True" || value == "TRUE" || value == "1") {
    return true;
  }
  if (value == "false" || value == "False" || value == "FALSE" ||
      value == "0") {
    return false;
  }
  auto result = FailureExecutionResult(
      core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED);
  SCP_ERROR(kConfigurationFetcherUtils, kZeroUuid, result,
            "Could not convert %s to bool", value.c_str());
  return static_cast<ExecutionResult>(result);
}
}  // namespace google::scp::cpio
