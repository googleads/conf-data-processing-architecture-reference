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

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/service_interface.h"
#include "core/common/concurrent_map/src/concurrent_map.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "public/cpio/utils/dual_writing_metric_client/interface/dual_writing_metric_client_interface.h"
#include "public/cpio/utils/key_fetching/interface/key_fetcher_with_cache_interface.h"
#include "public/cpio/utils/key_fetching/proto/key_coordinator_configuration.pb.h"

#include "error_codes.h"
#include "key_fetching_utils.h"

namespace google::scp::cpio {
enum class FetchingStatus { UNKNOWN = 0, IN_PROGRESS = 1, FINISHED = 2 };

///
/// @brief Auto-refresh Key cache for the coordinator's keys. This is only for
/// Hmac keys for now. NOTE: This cache caches keys forever without any eviction
/// policy. The expected usage of this is in situations where the number of keys
/// will not be significant.
/// The cache ensures the relevant keys are always available, right from the
/// point when Run() is invoked.
///
class AutoRefreshKeyFetcherWithCache : public KeyFetcherWithCacheInterface {
 public:
  AutoRefreshKeyFetcherWithCache(
      PrivateKeyClientInterface& key_client,
      const std::vector<
          google::cmrt::sdk::private_key_service::v1::PrivateKeyEndpoint>&
          private_key_endpoints,
      const std::string& keyset_name,
      DualWritingMetricClientInterface& metric_client,
      KeyFetcherOptions key_fetcher_options,
      const std::string& metric_namespace = {});

  core::ExecutionResultOr<std::vector<Key>> GetValidKeys(
      core::Timestamp key_selection_timestamp_ns =
          google::scp::core::common::TimeProvider::
              GetWallTimestampInNanosecondsAsClockTicks()) noexcept override;

  core::ExecutionResult Init() noexcept override;

  /// @brief Runs the cache refresher and makes the latest set of keys available
  /// immediately for use after this call.
  /// @return
  core::ExecutionResult Run() noexcept override;

  /// @brief Stops the cache refresher.
  /// @return
  core::ExecutionResult Stop() noexcept override;

 private:
  /// Auto-refresh the keys.
  void RefreshThreadLoopFunction() noexcept;

  void FetchAndCacheKeysetMetadataRemote(
      absl::string_view key_fetching_type) noexcept;

  bool HasEnoughValidKeyCached() noexcept;

  /**
   * @brief Fetch keys from KeyService with ListActiveEncryptionKeys API.
   * This is used for prefetching, auto-refreshing and also ondemand. The
   * fetching time range will be set to [now - 12hours, now + 7days].
   *
   * @param key_fetching_type key fetching type for metric recording.
   * time.
   * @return ExecutionResultOr<google::cmrt::sdk::private_key_service::v1::
   * ListActiveEncryptionKeysResponse>
   */
  core::ExecutionResultOr<google::cmrt::sdk::private_key_service::v1::
                              ListActiveEncryptionKeysResponse>
  FetchKeysFromRemoteWithActiveKeysApi(
      absl::string_view key_fetching_type) noexcept;

  /**
   * @brief Insert keys into key cache.
   *
   * @param keys keys to cache.
   */
  void CacheKeys(const std::vector<Key>& keys) noexcept;

  // Gets valid cached keys based on the specified timestamp.
  core::ExecutionResultOr<std::vector<Key>> GetKeysFromCache(
      core::Timestamp timestamp_ns) noexcept;

  /**
   * @brief Try to fetch from remote and cache the keys.
   *
   * @return ExecutionResultOr<std::vector<Key>> fetched keys.
   */
  core::ExecutionResultOr<std::vector<Key>> TryFetchAndCacheKeys(
      absl::string_view key_fetching_type) noexcept;

  core::ExecutionResult ValidateKeySelectionTimestamp(
      core::Timestamp key_selection_timestamp_ns) noexcept;

  /// Set the fetching status to FINISHED.
  void MarkFetchingFinished() noexcept;
  /**
   * @brief Try to set the fetching status to IN_PROGRESS when it is not yet.
   *
   * @return true made the update.
   * @return false the status is already IN_PROGRESS and the update is skipped.
   */
  bool MarkFetchingInProgress() noexcept;
  /// Wait for the key fetching finishing.
  void WaitForKeyReady() noexcept;
  /// Check if the fetching status is IN_PROGRESS.
  bool FetchingInProgress() noexcept;

  std::string MapToKeyFetchingErrorString(
      core::StatusCode status_code) noexcept;

  PrivateKeyClientInterface& key_client_;
  const std::string keyset_name_;
  const std::vector<
      google::cmrt::sdk::private_key_service::v1::PrivateKeyEndpoint>
      private_key_endpoints_;

  size_t refresh_duration_in_seconds_;
  DualWritingMetricClientInterface& metric_client_;
  KeyFetcherOptions key_fetcher_options_;

  std::thread refresh_thread_;
  absl::Notification stop_notification_;
  std::shared_mutex key_cache_mutex_;

  std::shared_mutex status_mutex_;
  std::condition_variable_any status_cv_;
  FetchingStatus key_fetching_status_;
  absl::btree_map<core::Timestamp, Key> key_cache_map_;

  std::atomic<int> keyset_active_key_count_{0};
  // The backfill days for the keyset, -1 means unfetched value.
  std::atomic<int> keyset_backfill_days_{-1};

};
}  // namespace google::scp::cpio
