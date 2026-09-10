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

#include <memory>

#include "cc/core/interface/async_executor_interface.h"
#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"

#include "coordinator_key_fetcher_with_cache_base.h"

namespace google::scp::cpio {

class OndemandKeyFetcherWithCache
    : public CoordinatorKeyFetcherWithCacheBase<std::string>,
      public KeyFetcherWithCacheInterface {
 public:
  explicit OndemandKeyFetcherWithCache(
      std::shared_ptr<google::scp::core::AsyncExecutorInterface>&
          async_executor,
      PrivateKeyClientInterface& key_client,
      DualWritingMetricClientInterface& metric_client,
      const google::cmrt::sdk::v1::KeyCoordinatorConfiguration&
          key_service_options,
      KeyFetcherOptions key_fetcher_options,
      const std::string& metric_namespace = {});

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResultOr<Key> GetKey(
      const std::string& key_id) noexcept override;

  core::ExecutionResultOr<bool> ValidateKey(
      const std::string& key_id) noexcept override;

 private:
  /// Cache valid keys.
  void CacheValidKey(const std::vector<Key>& valid_keys) noexcept override;

  /**
   * @brief Get the Key From Valid Key Cache
   *
   * @param key_id the given key ID
   * @return std::optional<Key> found key
   */
  std::optional<Key> GetKeyFromValidKeyCache(
      const std::string& key_id) noexcept override;

  /**
   * @brief Get the Key Fetching Failure from Cache for the Given Key ID
   *
   * @param key_id the given key ID
   * @return std::optional<ExecutionResult> found failure result
   */
  std::optional<core::ExecutionResult> GetFetchingFailureFromCache(
      const std::string& key_id) noexcept override;

  /**
   * @brief Cache failure result and key IDs for fetching failures
   *
   * @param key_id key ID
   * @param failure_result failure result
   */
  void CacheFailureResult(
      std::string key_id,
      core::ExecutionResult failure_result) noexcept override;

  /// Remove key_id from in_progress cache.
  void MarkFetchingFinished(const std::string& key_id) noexcept override;
  /**
   * @brief Add key_id to in_progress cache when it is not yet.
   *
   * @return true made the operation.
   * @return false the key_id is already in the in progress cache and skip add.
   */
  bool MarkFetchingInProgress(const std::string& key_id) noexcept override;
  /// Check if the key_id is in the in progress cache.
  bool FetchingInProgress(const std::string& key_id) noexcept override;

  /// Construct ListPrivateKeysRequest with key_id added to request_base.
  google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest
  GetListPrivateKeysRequest(
      const google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest&
          request_base,
      const std::string& key_id) const noexcept override;

  core::common::AutoExpiryConcurrentMap<std::string, Key> key_cache_;
  // A cache of key IDs and key fetching failures.
  core::common::AutoExpiryConcurrentMap<std::string, core::ExecutionResult>
      fetching_failure_cache_;

  std::shared_mutex in_progress_key_cache_mutex_;
  // Store the key IDs which a thread is fetching the key for.
  std::unordered_set<std::string> in_progress_key_cache_;
};
}  // namespace google::scp::cpio
