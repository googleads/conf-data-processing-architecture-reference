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

#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/strings/string_view.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "public/cpio/utils/dual_writing_metric_client/interface/dual_writing_metric_client_interface.h"
#include "public/cpio/utils/key_fetching/interface/key_fetcher_with_cache_interface.h"
#include "public/cpio/utils/key_fetching/proto/key_coordinator_configuration.pb.h"

#include "error_codes.h"

namespace google::scp::cpio {

class CoordinatorKeyFetcherWithCacheBase : public KeyFetcherWithCacheInterface {
 public:
  explicit CoordinatorKeyFetcherWithCacheBase(
      PrivateKeyClientInterface& key_client,
      DualWritingMetricClientInterface& metric_client,
      const google::cmrt::sdk::v1::KeyCoordinatorConfiguration&
          key_service_options,
      KeyFetcherOptions key_fetcher_options, absl::string_view component_name,
      absl::string_view key_type, const std::string& metric_namespace = {});

  core::ExecutionResultOr<Key> GetKey(
      const std::string& key_id) noexcept override;

  core::ExecutionResultOr<bool> ValidateKey(
      const std::string& key_id) noexcept override;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

 protected:
  /// Cache valid keys.
  virtual void CacheValidKey(const std::vector<Key>& valid_keys) noexcept = 0;

  /**
   * @brief Get the Key From Valid Key Cache
   *
   * @param key_id the given key ID
   * @return std::optional<Key> found key
   */
  virtual std::optional<Key> GetKeyFromValidKeyCache(
      const std::string& key_id) noexcept = 0;

  /**
   * @brief Get the Key Fetching Failure from Cache for the Given Key ID
   *
   * @param key_id the given key ID
   * @return std::optional<ExecutionResult> found failure result
   */
  virtual std::optional<core::ExecutionResult> GetFetchingFailureFromCache(
      const std::string& key_id) noexcept = 0;

  /**
   * @brief Cache failure result and key IDs for fetching failures
   *
   * @param key_id key ID
   * @param failure_result failure result
   */
  virtual void CacheFailureResult(
      std::string key_id, core::ExecutionResult failure_result) noexcept = 0;

  /// Remove key_id from in_progress cache.
  virtual void MarkFetchingFinished(const std::string& key_id) noexcept = 0;
  /**
   * @brief Add key_id to in_progress cache when it is not yet.
   *
   * @return true made the operation.
   * @return false the key_id is already in the in progress cache and skip add.
   */
  virtual bool MarkFetchingInProgress(const std::string& key_id) noexcept = 0;
  /// Check if the key_id is in the in progress cache.
  virtual bool FetchingInProgress(const std::string& key_id) noexcept = 0;

 private:
  // Get the key from valid key cache or fetch it from remote.
  core::ExecutionResultOr<Key> GetKeyInternal(
      const std::string& key_id) noexcept;

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
  core::ExecutionResultOr<Key> FetchValidateAndCacheKey(
      const std::string& key_id) noexcept;

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

  /// Wait for the key fetching finishing.
  void WaitForKeyReady(const std::string& key_id) noexcept;

  // Function to convert an error during key fetching to a string
  // for metric recording.
  std::string MapToKeyFetchingErrorString(
      core::StatusCode status_code) noexcept;

  PrivateKeyClientInterface& key_client_;

  const google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest
      list_private_keys_request_base_;
  const google::cmrt::sdk::private_key_service::v1::
      ListActiveEncryptionKeysRequest list_active_keys_request_base_;
  const std::set<std::string> allowed_keysets_list_;

  KeyFetcherOptions key_fetcher_options_;

  DualWritingMetricClientInterface& metric_client_;
  std::string allowed_keysets_name_;
  std::string component_name_;
  std::string key_type_;
};
}  // namespace google::scp::cpio
