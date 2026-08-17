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
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using std::string;
using std::vector;

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

ExecutionResultOr<vector<string>> ConfigurationFetcherUtils::StringToList(
    const string& value) {
  vector<string> list;
  if (value.empty()) {
    return list;
  }
  std::string str = value;
  str = std::string(absl::StripPrefix(str, "["));
  str = std::string(absl::StripSuffix(str, "]"));
  for (absl::string_view part : absl::StrSplit(str, ',', absl::SkipEmpty())) {
    std::string trimmed(absl::StripAsciiWhitespace(part));
    trimmed = std::string(absl::StripPrefix(trimmed, "\""));
    trimmed = std::string(absl::StripSuffix(trimmed, "\""));
    trimmed = std::string(absl::StripPrefix(trimmed, "'"));
    trimmed = std::string(absl::StripSuffix(trimmed, "'"));
    if (!trimmed.empty()) {
      list.push_back(trimmed);
    }
  }
  return list;
}
}  // namespace google::scp::cpio
