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

#pragma once

#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_set>

#include "absl/container/flat_hash_set.h"
#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "core/interface/async_executor_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/kms_client/kms_client_interface.h"
#include "public/cpio/utils/dual_writing_metric_client/interface/dual_writing_metric_client_interface.h"

#include "wrapped_key_handler_with_cache_interface.h"

namespace google::scp::cpio {

template <typename WrappedKeyType>
class WrappedKeyHandlerWithCacheBase
    : public WrappedKeyHandlerWithCacheInterface {
 public:
  WrappedKeyHandlerWithCacheBase(
      std::shared_ptr<google::scp::core::AsyncExecutorInterface>&
          async_executor,
      KmsClientInterface& kms_client,
      WrappedKeyHandlerOptions wrapped_key_handler_options,
      DualWritingMetricClientInterface& metric_client);

  google::scp::core::ExecutionResult Init() noexcept override;

  google::scp::core::ExecutionResult Run() noexcept override;

  google::scp::core::ExecutionResult Stop() noexcept override;

 protected:
  google::scp::core::ExecutionResultOr<std::string> GetKeyInternal(
      const WrappedKeyType& wrapped_key) noexcept;

  std::string SerializeWrappedKey(const WrappedKeyType& wrapped_key) noexcept;

  std::string WrappedKeyToDebugString(
      const WrappedKeyType& wrapped_key) noexcept;

  virtual std::string GetKeyType() noexcept = 0;
  virtual std::string GetKekPrefix() noexcept = 0;

  virtual google::scp::core::ExecutionResultOr<
      google::cmrt::sdk::kms_service::v1::DecryptRequest>
  CreateDecryptRequest(const WrappedKeyType& wrapped_key) noexcept = 0;

  /**
   * @brief Decrypt DEK by calling KMS client, validate the decrypted DEK, and
   * cache it in either valid key or failed key caches.
   *
   * @param wrapped_key WrappedKeyType proto
   * @return decrypted_dek or failure
   */
  google::scp::core::ExecutionResultOr<std::string>
  DecryptValidateAndCacheDecryptedDek(
      const WrappedKeyType& wrapped_key) noexcept;

  /**
   * @brief Validate KMS decrypt response and cache the valid decrypted_dek or
   * cache the failure.
   *
   * @param wrapped_key WrappedKeyType proto
   * @param decrypt_response_or KMS decrypt response contained decrypted_dek
   * @return valid decrypted_dek or failure
   */
  google::scp::core::ExecutionResultOr<std::string>
  ValidateAndCacheDecryptedDek(
      const WrappedKeyType& wrapped_key,
      const google::scp::core::ExecutionResultOr<
          google::cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_dek_response_or) noexcept;

  /// Cache wrapped keys and their decrypted_deks in key_cache_
  void CacheValidKey(const std::string& serialized_wrapped_key,
                     const std::string& decrypted_dek) noexcept;

  /// Cache wrapped keys with failure results in key_failure_cache_
  void CacheFailureResultForWrappedKey(
      std::string serialized_wrapped_key,
      google::scp::core::ExecutionResult failure_result) noexcept;

  // Get decrypted_dek from valid key_cache_
  std::optional<std::string> GetDecryptedDekFromValidKeyCache(
      const std::string& serialized_wrapped_key) noexcept;

  /// Get decryption failure for given wrapped key from key_failure_cache_.
  std::optional<google::scp::core::ExecutionResult>
  GetDecryptionFailureFromCache(const WrappedKeyType& wrapped_key) noexcept;

  /// Remove wrapped_key from in_progress cache.
  void MarkDecryptionFinished(
      const std::string& serialized_wrapped_key) noexcept;

  /**
   * @brief Add wrapped_key to in_progress cache when it is not yet.
   *
   * @return true made the operation.
   * @return false the wrapped_key is already in the in progress cache and skip
   * add.
   */
  bool MarkDecryptionInProgress(
      const std::string& serialized_wrapped_key) noexcept;

  /// Wait for the key decryption to finish.
  void WaitForKeyReady(const std::string& serialized_wrapped_key) noexcept;

  /// Check if the wrapped_key is in the in_progress cache.
  bool DecryptionInProgress(const std::string& serialized_wrapped_key) noexcept;

  // Function to translate ExecutionResult status code into a string for metric
  // recording.
  virtual std::string MapToWrappedKeyFetchingErrorString(
      google::scp::core::ExecutionResult error_result) noexcept = 0;

  virtual bool IsRetryableDecryptionError(
      google::scp::core::StatusCode error_code) noexcept = 0;

  /// A cache of serialized wrapped keys and their corresponding decrypted_dek.
  google::scp::core::common::AutoExpiryConcurrentMap<std::string, std::string>
      key_cache_;

  /// A cache of serialized wrapped keys and DEK decryption failures.
  google::scp::core::common::AutoExpiryConcurrentMap<
      std::string, google::scp::core::ExecutionResult>
      key_failure_cache_;

  /// Store serialized wrapped keys that a thread is already decrypting.
  std::unordered_set<std::string> in_progress_key_cache_;

  std::shared_mutex in_progress_key_cache_mutex_;  // NOLINT(build/c++14)

  KmsClientInterface& kms_client_;

  WrappedKeyHandlerOptions wrapped_key_handler_options_;

  DualWritingMetricClientInterface& metric_client_;
};

}  // namespace google::scp::cpio
