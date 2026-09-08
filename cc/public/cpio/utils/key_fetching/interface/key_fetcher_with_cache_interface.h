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
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/key_fetching/proto/encryption_key_prefetch_config.pb.h"

namespace google::scp::cpio {

struct KeyFetcherOptions {
  bool prefetch_keys = false;
  bool prefetch_retry = false;
  uint64_t max_prefetch_wait_time_millis = 10 * 1000;
  std::chrono::seconds prefetch_keys_max_age =
      std::chrono::seconds(14 * 24 * 60 * 60);
  std::chrono::seconds key_cache_lifetime =
      std::chrono::seconds(14 * 24 * 60 * 60);
  std::chrono::seconds fetching_failure_cache_lifetime =
      std::chrono::seconds(5 * 60);
  bool enable_key_fetching_metrics = true;
  bool enable_active_keys_api_for_encryption_keys = true;

  std::chrono::seconds auto_refresh_time_duration =
      std::chrono::seconds(24 * 60 * 60);

  bool enable_on_demand_fetching_for_hmac_key = true;
  bool enable_on_demand_fetching_lock_for_encryption_key = true;
  std::chrono::milliseconds on_demand_fetching_waiting_timeout =
      std::chrono::milliseconds(2000);  // 2s
  bool use_read_lock_for_cache_read = false;

  // The validity period of the HMAC key stored in the HMAC cache is calculated
  // in days, meaning the cache will ensure it remains valid for the next few
  // days. Default value is 1 day, which means the cache will ensure the HMAC
  // key remains valid for the next 1 day. This is a safety measure to prevent
  // using expired keys from the cache, while also allowing for some flexibility
  // in key rotation. The value can be adjusted based on the expected key
  // rotation frequency and the acceptable risk of using slightly older keys.
  uint64_t auto_refresh_key_cache_valid_in_days = 1;

  // Map of keyset namespace to its given prefetch configuration.
  absl::flat_hash_map<
      std::string,
      google::cmrt::sdk::v1::EncryptionKeyPrefetchConfig::KeysetPrefetchConfig>
      encryption_key_prefetch_config_map;

  // Enable the validation of key selection timestamp.
  bool enable_key_selection_timestamp_validation = true;
};

/// @brief Key returned by KeyFetcherWithCacheInterface
struct Key {
  /// @brief Creation timestamp of the key. UTC time in nanos resolution since
  /// epoch.
  core::Timestamp creation_timestamp;
  /// @brief Expiration timestamp of the key. UTC time in nanos resolution since
  /// epoch.
  core::Timestamp expiration_timestamp;
  /// @brief Activation timestamp of the key. UTC time in nanos resolution since
  /// epoch.
  core::Timestamp activation_timestamp;
  /// @brief Private Key data
  std::string private_key;
  /// @brief Public Key data
  std::string public_key;
  // @brief key_id of the key.
  std::string key_id;
};

/// @brief Interface to cache a set of keys from an key management service.
class KeyFetcherWithCacheInterface
    : public google::scp::core::ServiceInterface {
 public:
  virtual ~KeyFetcherWithCacheInterface() = default;

  /// @brief Obtain a key corresponding to the Key ID.
  /// @return Key or key not found error.
  virtual core::ExecutionResultOr<Key> GetKey(
      const std::string& key_id) noexcept {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  /// @brief Retrieves valid active keys for given timestamp.
  /// @param key_selection_timestamp_ns The timestamp to select valid keys. This
  /// is optional field for key selection algorithm that requires a timestamp.
  /// If not provided, the current wall time will be used.
  /// NOTE: This list must be sorted in ascending order by the keys'
  /// expiration_timestamp.
  /// @return Valid key list or key not found error.
  virtual core::ExecutionResultOr<std::vector<Key>> GetValidKeys(
      core::Timestamp key_selection_timestamp_ns = google::scp::core::common::
          TimeProvider::GetWallTimestampInNanosecondsAsClockTicks()) noexcept {
    // Default not implemented error
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  /// @brief Validate a key is allowed for this application type.
  /// @return Key or key not found error.
  virtual core::ExecutionResultOr<bool> ValidateKey(
      const std::string& key_id) noexcept {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }
};

}  // namespace google::scp::cpio
