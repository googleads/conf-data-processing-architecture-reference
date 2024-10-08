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

#include "public_key_client_utils.h"

#include <locale>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/str_split.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED;
using std::get_time;
using std::istringstream;
using std::mktime;
using std::regex;
using std::regex_replace;
using std::stoi;
using std::string;
using std::tm;
using std::vector;

namespace {
constexpr char kPublicKeyClientUtils[] = "PublicKeyClientUtils";
constexpr char kPublicKeysLabel[] = "keys";
constexpr char kPublicKeyIdLabel[] = "id";
constexpr char kPublicKeyLabel[] = "key";
constexpr char kPublicKeyHeaderDate[] = "date";
constexpr char kPublicKeyHeaderCacheControl[] = "cache-control";
constexpr char kPublicKeyDateTimeFormat[] = "%a, %d %b %Y %H:%M:%S";
constexpr char kPublicKeyMaxAgePrefix1[] = "max-age=";
constexpr char kPublicKeyMaxAgePrefix2[] = " max-age=";
}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResult PublicKeyClientUtils::ParseExpiredTimeFromHeaders(
    const HttpHeaders& headers, uint64_t& expired_time_in_s) noexcept {
  auto created_date = headers.find(kPublicKeyHeaderDate);
  auto cache_control = headers.find(kPublicKeyHeaderCacheControl);
  if (created_date == headers.end() || cache_control == headers.end()) {
    auto failure = FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
    SCP_ERROR(kPublicKeyClientUtils, kZeroUuid, failure,
              "No created date or cache control in the header.");
    return failure;
  }
  tm time_date = {};
  istringstream stream_time(created_date->second);
  stream_time >> get_time(&time_date, kPublicKeyDateTimeFormat);
  vector<string> splits = absl::StrSplit(cache_control->second, ",");
  string max_age;
  for (auto& split : splits) {
    if (split.rfind(kPublicKeyMaxAgePrefix1, 0) == 0) {
      max_age = regex_replace(split, std::regex(kPublicKeyMaxAgePrefix1), "");
      break;
    }
    if (split.rfind(kPublicKeyMaxAgePrefix2, 0) == 0) {
      max_age = regex_replace(split, std::regex(kPublicKeyMaxAgePrefix2), "");
      break;
    }
  }
  if (max_age.empty()) {
    auto failure = FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
    SCP_ERROR(kPublicKeyClientUtils, kZeroUuid, failure,
              "No max-age in cache control header.");
    return failure;
  }

  auto mt_time = mktime(&time_date);
  if (mt_time < 0) {
    auto failure = FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
    SCP_ERROR(kPublicKeyClientUtils, kZeroUuid, failure,
              "Invalid time format for created date in the header: %s",
              created_date->second.c_str());
    return failure;
  }

  try {
    expired_time_in_s = mt_time + stoi(max_age);
  } catch (...) {
    auto failure = FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
    SCP_ERROR(kPublicKeyClientUtils, kZeroUuid, failure,
              "Invalid max-age value in the header: %s", max_age.c_str());
    return failure;
  }

  return SuccessExecutionResult();
}

ExecutionResult PublicKeyClientUtils::ParsePublicKeysFromBody(
    const BytesBuffer& body, vector<PublicKey>& public_keys) noexcept {
  auto json_response =
      nlohmann::json::parse(body.bytes->begin(), body.bytes->end());
  auto json_keys = json_response.find(kPublicKeysLabel);
  if (json_keys == json_response.end()) {
    return FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
  }

  auto key_count = json_keys.value().size();
  for (size_t i = 0; i < key_count; ++i) {
    auto json_str = json_keys.value()[i];

    public_keys.emplace_back();
    auto it = json_str.find(kPublicKeyIdLabel);
    if (it == json_str.end()) {
      return FailureExecutionResult(
          SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
    }
    public_keys.back().set_key_id(it.value());

    it = json_str.find(kPublicKeyLabel);
    if (it == json_str.end()) {
      return FailureExecutionResult(
          SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
    }
    public_keys.back().set_public_key(it.value());
  }

  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio::client_providers
