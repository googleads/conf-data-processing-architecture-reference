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

/**
 * @brief Base class for cache that fetch keys from key coordinators.
 *
 * @tparam LookupKeyT The type of the key used to look up keys in the cache.
 * Possible types are `std::string` (key IDs for cache of encryption keys) and
 * `core::Timestamp` (timestamps for cache of SID keys).
 */
template <typename LookupKeyT>
class CoordinatorKeyFetcherWithCacheBase {
 public:
  explicit CoordinatorKeyFetcherWithCacheBase(
      PrivateKeyClientInterface& key_client,
      DualWritingMetricClientInterface& metric_client,
      const google::cmrt::sdk::v1::KeyCoordinatorConfiguration&
          key_service_options,
      KeyFetcherOptions key_fetcher_options, absl::string_view component_name,
      absl::string_view key_type, const std::string& metric_namespace = {});

  virtual ~CoordinatorKeyFetcherWithCacheBase() = default;

  core::ExecutionResult Init() noexcept;

  core::ExecutionResult Run() noexcept;

  core::ExecutionResult Stop() noexcept;

 protected:
  /// Cache valid keys.
  virtual void CacheValidKey(const std::vector<Key>& valid_keys) noexcept = 0;

  /**
   * @brief Get the Key From Valid Key Cache
   *
   * @param lookup_key key used to lookup in the cache, can be string for key id
   * or timestamp
   * @return std::optional<Key> found key
   */
  virtual std::optional<Key> GetKeyFromValidKeyCache(
      const LookupKeyT& lookup_key) noexcept = 0;

  /**
   * @brief Get the Key Fetching Failure from Cache for the Given Lookup Key
   *
   * @param lookup_key key used to lookup in the cache, can be string for key id
   * or timestamp
   * @return std::optional<ExecutionResult> found failure result
   */
  virtual std::optional<core::ExecutionResult> GetFetchingFailureFromCache(
      const LookupKeyT& lookup_key) noexcept = 0;

  /**
   * @brief Cache failure result and lookup key for fetching failures
   *
   * @param lookup_key key used to lookup in the cache, can be string for key id
   * or timestamp
   * @param failure_result failure result
   */
  virtual void CacheFailureResult(
      LookupKeyT lookup_key, core::ExecutionResult failure_result) noexcept = 0;

  /// Mark key fetching status as finished.
  virtual void MarkFetchingFinished(const LookupKeyT& lookup_key) noexcept = 0;
  /**
   * @brief Mark the key fetching status as in progress when it is not yet.
   *
   * @return true made the operation.
   * @return false the status is already in progress and skip add.
   */
  virtual bool MarkFetchingInProgress(
      const LookupKeyT& lookup_key) noexcept = 0;
  /// Check if the key fetching is in progress.
  virtual bool FetchingInProgress(const LookupKeyT& lookup_key) noexcept = 0;

  /// Construct ListPrivateKeysRequest with lookup_key added to request_base.
  virtual google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest
  GetListPrivateKeysRequest(
      const google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest&
          request_base,
      const LookupKeyT& lookup_key) const noexcept = 0;

  // Get the key from valid key cache or fetch it from remote.
  core::ExecutionResultOr<Key> GetKeyInternal(
      const LookupKeyT& lookup_key) noexcept;

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
   * @param lookup_key key used to lookup in the cache, can be string for key id
   * or timestamp
   * @return ExecutionResultOr<Key> fetch and validate result
   */
  core::ExecutionResultOr<Key> FetchValidateAndCacheKey(
      const LookupKeyT& lookup_key) noexcept;

  /**
   * @brief Validate list keys result and cache the valid key or cache the
   * failure.
   *
   * @param lookup_key key used to lookup in the cache, can be string for key id
   * or timestamp
   * @param list_keys_response_or list keys result
   * @return ExecutionResultOr<Key> cached valid key or failure
   */
  core::ExecutionResultOr<Key> ValidateAndCacheKey(
      const LookupKeyT lookup_key,
      const core::ExecutionResultOr<
          google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          list_keys_response_or) noexcept;

  /// Wait for the key fetching finishing.
  void WaitForKeyReady(const LookupKeyT& lookup_key) noexcept;

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
