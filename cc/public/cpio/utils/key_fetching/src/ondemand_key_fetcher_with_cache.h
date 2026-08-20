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

#include <chrono>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/service_interface.h"
#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "public/cpio/utils/dual_writing_metric_client/interface/dual_writing_metric_client_interface.h"
#include "public/cpio/utils/key_fetching/interface/key_fetcher_with_cache_interface.h"
#include "public/cpio/utils/key_fetching/proto/encryption_key_prefetch_config.pb.h"
#include "public/cpio/utils/key_fetching/proto/key_coordinator_configuration.pb.h"

#include "error_codes.h"
#include "key_fetching_utils.h"

namespace google::scp::cpio {

class OndemandKeyFetcherWithCache : public KeyFetcherWithCacheInterface {
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

  core::ExecutionResultOr<Key> GetKey(
      const std::string& key_id) noexcept override;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

 private:
  // The input keyset_name is only used for metrics.
  core::ExecutionResultOr<
      google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>
  FetchKeysFromRemote(
      const google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest&
          request,
      absl::string_view key_fetching_type, absl::string_view keyset_name);

  core::ExecutionResultOr<google::cmrt::sdk::private_key_service::v1::
                              ListActiveEncryptionKeysResponse>
  FetchKeysFromRemoteWithActiveKeysApi(
      const google::cmrt::sdk::private_key_service::v1::
          ListActiveEncryptionKeysRequest& request,
      absl::string_view key_fetching_type, absl::string_view keyset_name);

  /**
   * @brief Validate the fetched key and cache it in valid key cache.
   *
   * @param fetched_key fetched key.
   * @return ExecutionResultOr<Key> cached key.
   */
  core::ExecutionResultOr<Key> ValidateAndCachePrivateKey(
      google::cmrt::sdk::private_key_service::v1::PrivateKey&
          fetched_key) noexcept;

  // Helper function to sleep a random duration.
  void SleepRandomDuration() noexcept;

  /// Prefetch recent keys.
  void PrefetchKeys() noexcept;

  // Helper function for PrefetchKeys().
  void PrefetchWithListActiveKeys(
      const std::string& keyset_name,
      const google::protobuf::Timestamp& start_time,
      const google::protobuf::Timestamp& end_time) noexcept;

  // Helper function for PrefetchKeys().
  void PrefetchWithListPrivateKeys(
      const std::string& keyset_name,
      const std::optional<google::protobuf::RepeatedPtrField<std::string>>&
          key_ids) noexcept;

  /**
   * @brief Fetch key from remote, validate the key and cache the key in valid
   * key or failed key caches.
   *
   * @param key_id the given key ID
   * @return ExecutionResultOr<Key> fetch and validate result
   */
  core::ExecutionResultOr<Key> FetchValidateAndCacheKeyById(
      const std::string& key_id) noexcept;

  /// Cache valid keys.
  void CacheValidKey(const std::vector<Key>& valid_keys) noexcept;

  /**
   * @brief Validate list keys result and cache the valid key or cache the
   * failure.
   *
   * @param key_id key ID
   * @param list_keys_response_or list keys result
   * @return ExecutionResultOr<Key> cached valid key or failure
   */
  core::ExecutionResultOr<Key> ValidateAndCacheKey(
      const std::string key_id,
      const core::ExecutionResultOr<
          google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          list_keys_response_or) noexcept;

  /**
   * @brief Get the Key From Valid Key Cache
   *
   * @param key_id the given key ID
   * @return std::optional<Key> found key
   */
  std::optional<Key> GetKeyFromValidKeyCache(
      const std::string& key_id) noexcept;

  /**
   * @brief Get the Key Fetching Failure from Cache for the Given Key ID
   *
   * @param key_id the given key ID
   * @return std::optional<ExecutionResult> found failure result
   */
  std::optional<core::ExecutionResult> GetFetchingFailureFromCache(
      const std::string& key_id) noexcept;

  /**
   * @brief Cache failure result and key IDs for fetching failures
   *
   * @param key_id key ID
   * @param failure_result failure result
   */
  void CacheFailureResultForKeyId(
      std::string key_id, core::ExecutionResult failure_result) noexcept;

  /// Remove key_id to in_progress cache.
  void MarkFetchingFinished(const std::string& key_id) noexcept;
  /**
   * @brief Add key_id to in_progress cache when it is not yet.
   *
   * @return true made the operation.
   * @return false the key_id is already in the in progress cache and skip add.
   */
  bool MarkFetchingInProgress(const std::string& key_id) noexcept;
  /// Wait for the key fetching finishing.
  void WaitForKeyReady(const std::string& key_id) noexcept;
  /// Check if the key_id is in the in progress cache.
  bool FetchingInProgress(const std::string& key_id) noexcept;

  // Function to convert an error during key fetching to a string
  // for metric recording.
  std::string MapToKeyFetchingErrorString(
      core::StatusCode status_code) noexcept;

  core::common::AutoExpiryConcurrentMap<std::string, Key> key_cache_;
  // A cache of key IDs and key fetching failures.
  core::common::AutoExpiryConcurrentMap<std::string, core::ExecutionResult>
      fetching_failure_cache_;

  PrivateKeyClientInterface& key_client_;

  const google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest
      list_private_keys_request_base_;
  const google::cmrt::sdk::private_key_service::v1::
      ListActiveEncryptionKeysRequest list_active_keys_request_base_;
  const std::set<std::string> allowed_keysets_list_;

  KeyFetcherOptions key_fetcher_options_;

  DualWritingMetricClientInterface& metric_client_;
  std::string allowed_keysets_name_;

  std::shared_mutex in_progress_key_cache_mutex_;
  // Store the key IDs which a thread is fetching the key for.
  std::unordered_set<std::string> in_progress_key_cache_;
};
}  // namespace google::scp::cpio
