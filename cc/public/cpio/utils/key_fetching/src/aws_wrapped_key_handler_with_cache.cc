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

#include "aws_wrapped_key_handler_with_cache.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "core/interface/async_executor_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/core/interface/execution_result_macros.h"
#include "public/cpio/interface/error_codes.h"
#include "public/cpio/interface/kms_client/kms_client_interface.h"
#include "public/cpio/utils/dual_writing_metric_client/interface/dual_writing_metric_client_interface.h"
#include "public/cpio/utils/key_fetching/src/error_codes.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::v1::AwsWrappedKey;
using google::cmrt::sdk::v1::CloudWrappedKey;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::errors::SC_CPIO_ENTITY_NOT_FOUND;
using google::scp::core::errors::SC_CPIO_INTERNAL_ERROR;
using google::scp::core::errors::SC_CPIO_INVALID_ARGUMENT;
using google::scp::core::errors::SC_CPIO_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_CPIO_KEY_NOT_FOUND;
using google::scp::core::errors::SC_CPIO_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_PROTO_PARSING_FAILURE;
using google::scp::cpio::DualWritingMetricClientInterface;
using google::scp::cpio::KeyType;
using google::scp::cpio::KmsClientInterface;
using std::shared_ptr;
using std::string;
using std::chrono::milliseconds;

namespace google::scp::cpio {

namespace {
constexpr char kAwsWrappedKeyHandlerWithCacheComponentName[] =
    "AwsWrappedKeyHandlerWithCache";
constexpr char tinkKekAwsPrefix[] = "aws-kms://";
constexpr milliseconds kLogPeriod = milliseconds(1000);
}  // namespace

AwsWrappedKeyHandlerWithCache::AwsWrappedKeyHandlerWithCache(
    shared_ptr<AsyncExecutorInterface>& async_executor,
    KmsClientInterface& kms_client,
    WrappedKeyHandlerOptions wrapped_key_handler_options,
    DualWritingMetricClientInterface& metric_client)
    : WrappedKeyHandlerWithCacheBase(async_executor, kms_client,
                                     wrapped_key_handler_options,
                                     metric_client) {}

string AwsWrappedKeyHandlerWithCache::GetKeyType() noexcept {
  return KeyType::kAwsWrappedKey;
}

string AwsWrappedKeyHandlerWithCache::GetKekPrefix() noexcept {
  return tinkKekAwsPrefix;
}

ExecutionResultOr<string> AwsWrappedKeyHandlerWithCache::GetKey(
    const CloudWrappedKey& wrapped_key) noexcept {
  if (!wrapped_key.has_aws_wrapped_key()) {
    auto result = FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT);
    SCP_ERROR_EVERY_PERIOD(
        kLogPeriod, kAwsWrappedKeyHandlerWithCacheComponentName, kZeroUuid,
        result, "Wrapped key does not have aws_wrapped_key.");
    return result;
  }
  auto aws_wrapped_key = wrapped_key.aws_wrapped_key();
  if (aws_wrapped_key.encrypted_dek().empty() ||
      aws_wrapped_key.kek_uri().empty() || aws_wrapped_key.role_arn().empty()) {
    auto result = FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT);
    SCP_ERROR_EVERY_PERIOD(
        kLogPeriod, kAwsWrappedKeyHandlerWithCacheComponentName, kZeroUuid,
        result,
        "AWS wrapped key is missing encrypted_dek, kek_uri, or role_arn.");
    return result;
  }
  aws_wrapped_key.set_kek_uri(
      string(absl::StripPrefix(aws_wrapped_key.kek_uri(), GetKekPrefix())));
  return GetKeyInternal(aws_wrapped_key);
}

ExecutionResultOr<DecryptRequest>
AwsWrappedKeyHandlerWithCache::CreateDecryptRequest(
    const AwsWrappedKey& wrapped_key) noexcept {
  DecryptRequest decrypt_request;
  decrypt_request.set_ciphertext(wrapped_key.encrypted_dek());
  decrypt_request.set_key_resource_name(wrapped_key.kek_uri());

  // Must include region
  std::vector<absl::string_view> resource_parts =
      absl::StrSplit(decrypt_request.key_resource_name(), ":");
  // Region is in the 3rd index. Eg: `arn:aws:kms:<region>:<###>`
  if (resource_parts.size() <= 3) {
    auto result = FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT);
    SCP_ERROR_EVERY_PERIOD(
        kLogPeriod, kAwsWrappedKeyHandlerWithCacheComponentName, kZeroUuid,
        result, "AWS KEK URI format is invalid: %s",
        decrypt_request.key_resource_name().c_str());
    return result;
  }
  decrypt_request.set_kms_region(std::string(resource_parts[3]));

  decrypt_request.set_account_identity(wrapped_key.role_arn());
  return decrypt_request;
}

string AwsWrappedKeyHandlerWithCache::MapToWrappedKeyFetchingErrorString(
    google::scp::core::ExecutionResult error_result) noexcept {
  auto public_error_code = GetPublicErrorCode(error_result.status_code);
  if (public_error_code == SC_CPIO_REQUEST_LIMIT_REACHED) {
    return KeyFetchingErrorType::kCustomerQuotaExceeded;
  }
  if (public_error_code == SC_CPIO_INVALID_CREDENTIALS) {
    return KeyFetchingErrorType::kCustomerKeyPermissionDenied;
  }
  if (public_error_code == SC_CPIO_KEY_NOT_FOUND ||
      public_error_code == SC_CPIO_INTERNAL_ERROR ||
      public_error_code == SC_CPIO_INVALID_ARGUMENT ||
      public_error_code == SC_CPIO_ENTITY_NOT_FOUND) {
    return KeyFetchingErrorType::kInvalidKeyId;
  }
  return KeyFetchingErrorType::kGenericError;
}

bool AwsWrappedKeyHandlerWithCache::IsRetryableDecryptionError(
    StatusCode error_code) noexcept {
  static const absl::NoDestructor<absl::flat_hash_set<StatusCode>>
      kNonRetryableDecryptionErrors(
          {SC_CPIO_KEY_NOT_FOUND, SC_CPIO_ENTITY_NOT_FOUND,
           SC_CPIO_INVALID_ARGUMENT, SC_PROTO_PARSING_FAILURE,
           SC_CPIO_INTERNAL_ERROR, SC_CPIO_INVALID_CREDENTIALS});
  return !kNonRetryableDecryptionErrors->contains(error_code);
}

}  // namespace google::scp::cpio
