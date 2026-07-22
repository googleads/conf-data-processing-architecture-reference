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

#include "gcp_wrapped_key_handler_with_cache.h"

#include <chrono>

#include "absl/base/no_destructor.h"
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
using google::cmrt::sdk::v1::CloudWrappedKey;
using google::cmrt::sdk::v1::GcpWrappedKey;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::common::kZeroUuid;
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
constexpr char kGcpWrappedKeyHandlerWithCacheComponentName[] =
    "GcpWrappedKeyHandlerWithCache";
constexpr char tinkKekGcpPrefix[] = "gcp-kms://";
constexpr milliseconds kLogPeriod = milliseconds(1000);
}  // namespace

GcpWrappedKeyHandlerWithCache::GcpWrappedKeyHandlerWithCache(
    shared_ptr<AsyncExecutorInterface>& async_executor,
    KmsClientInterface& kms_client,
    WrappedKeyHandlerOptions wrapped_key_handler_options,
    DualWritingMetricClientInterface& metric_client)
    : WrappedKeyHandlerWithCacheBase(async_executor, kms_client,
                                     wrapped_key_handler_options,
                                     metric_client) {}

string GcpWrappedKeyHandlerWithCache::GetKeyType() noexcept {
  return KeyType::kGcpWrappedKey;
}

string GcpWrappedKeyHandlerWithCache::GetKekPrefix() noexcept {
  return tinkKekGcpPrefix;
}

ExecutionResultOr<string> GcpWrappedKeyHandlerWithCache::GetKey(
    const CloudWrappedKey& wrapped_key) noexcept {
  if (!wrapped_key.has_gcp_wrapped_key()) {
    auto result = FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT);
    SCP_ERROR_EVERY_PERIOD(
        kLogPeriod, kGcpWrappedKeyHandlerWithCacheComponentName, kZeroUuid,
        result, "Wrapped key does not have gcp_wrapped_key.");
    return result;
  }
  auto gcp_wrapped_key = wrapped_key.gcp_wrapped_key();
  if (gcp_wrapped_key.encrypted_dek().empty() ||
      gcp_wrapped_key.kek_uri().empty() ||
      gcp_wrapped_key.wip_provider().empty()) {
    auto result = FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT);
    SCP_ERROR_EVERY_PERIOD(
        kLogPeriod, kGcpWrappedKeyHandlerWithCacheComponentName, kZeroUuid,
        result,
        "GCP wrapped key is missing encrypted_dek, kek_uri, or wip_provider.");
    return result;
  }
  gcp_wrapped_key.set_kek_uri(
      string(absl::StripPrefix(gcp_wrapped_key.kek_uri(), GetKekPrefix())));
  return GetKeyInternal(gcp_wrapped_key);
}

ExecutionResultOr<DecryptRequest>
GcpWrappedKeyHandlerWithCache::CreateDecryptRequest(
    const GcpWrappedKey& gcp_wrapped_key) noexcept {
  DecryptRequest decrypt_request;
  decrypt_request.set_ciphertext(gcp_wrapped_key.encrypted_dek());
  decrypt_request.set_key_resource_name(gcp_wrapped_key.kek_uri());
  decrypt_request.set_gcp_wip_provider(gcp_wrapped_key.wip_provider());
  return decrypt_request;
}

string GcpWrappedKeyHandlerWithCache::MapToWrappedKeyFetchingErrorString(
    google::scp::core::ExecutionResult error_result) noexcept {
  if (error_result.status_code == SC_CPIO_REQUEST_LIMIT_REACHED) {
    return KeyFetchingErrorType::kCustomerQuotaExceeded;
  }
  if (error_result.status_code == SC_CPIO_INVALID_CREDENTIALS) {
    return KeyFetchingErrorType::kCustomerKeyPermissionDenied;
  }
  if (error_result.status_code == SC_CPIO_KEY_NOT_FOUND ||
      error_result.status_code == SC_CPIO_INTERNAL_ERROR ||
      error_result.status_code == SC_CPIO_INVALID_ARGUMENT ||
      error_result.status_code == SC_CPIO_ENTITY_NOT_FOUND) {
    return KeyFetchingErrorType::kInvalidKeyId;
  }
  return KeyFetchingErrorType::kGenericError;
}

bool GcpWrappedKeyHandlerWithCache::IsRetryableDecryptionError(
    StatusCode error_code) noexcept {
  static const absl::NoDestructor<absl::flat_hash_set<StatusCode>>
      kNonRetryableDecryptionErrors(
          {SC_CPIO_KEY_NOT_FOUND, SC_CPIO_ENTITY_NOT_FOUND,
           SC_CPIO_INVALID_ARGUMENT, SC_PROTO_PARSING_FAILURE,
           SC_CPIO_INTERNAL_ERROR, SC_CPIO_INVALID_CREDENTIALS});
  return !kNonRetryableDecryptionErrors->contains(error_code);
}

}  // namespace google::scp::cpio
