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

#include "auto_refresh_key_fetcher_with_cache.h"

#include <memory>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "core/common/time_provider/src/time_provider.h"
#include "public/core/interface/execution_result_macros.h"
#include "public/core/interface/execution_result_or_macros.h"
#include "public/cpio/interface/error_codes.h"
#include "public/cpio/utils/key_fetching/src/error_codes.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_utils.h"
#include "public/cpio/utils/metric_instance/src/metric_utils.h"

using google::cmrt::sdk::metric_service::v1::MetricType;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::private_key_service::v1::GetKeysetMetadataRequest;
using google::cmrt::sdk::private_key_service::v1::GetKeysetMetadataResponse;
using google::cmrt::sdk::private_key_service::v1::
    ListActiveEncryptionKeysRequest;
using google::cmrt::sdk::private_key_service::v1::
    ListActiveEncryptionKeysResponse;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::protobuf::Duration;
using google::protobuf::RepeatedPtrField;
using google::protobuf::util::TimeUtil;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::SC_CPIO_ENTITY_NOT_FOUND;
using google::scp::core::errors::SC_CPIO_INVALID_ARGUMENT;
using google::scp::core::errors::SC_CPIO_KEY_FETCHER_CACHE_INSERT_FAILURE;
using google::scp::core::errors::
    SC_CPIO_KEY_FETCHER_INVALID_KEY_SELECTION_TIMESTAMP;
using google::scp::core::errors::SC_CPIO_KEY_FETCHER_NO_KEY_FETCHED;
using google::scp::core::errors::SC_CPIO_KEY_NOT_FOUND;
using google::scp::cpio::DualWritingMetricClientInterface;
using google::scp::cpio::KeyCacheStatus;
using google::scp::cpio::KeyFetchingType;
using google::scp::cpio::KeyType;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricUtils;
using google::scp::cpio::PrivateKeyClientInterface;
using google::scp::cpio::PushKeyCacheStatusMetric;
using google::scp::cpio::PushKeyFetchingErrorMetric;
using google::scp::cpio::PushKeyFetchingLatencyMetric;
using google::scp::cpio::PushKeyFetchingRequestMetric;
using std::make_pair;
using std::mt19937;
using std::random_device;
using std::string;
using std::uniform_int_distribution;
using std::unordered_set;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;

namespace google::scp::cpio {
namespace {
constexpr char kAutoRefreshKeyFetcherWithCacheComponentName[] =
    "AutoRefreshKeyFetcherWithCache";
constexpr char kKeyFetchingRequestMetricsName[] = "KeyFetchingRequest";
constexpr char kKeyFetchingErrorMetricsName[] = "KeyFetchingError";
constexpr char kKeyFetchingLegacyLatencyMetricName[] = "KeyFetchingLatencyInMs";
constexpr char kPrefetchRetryCountMetricName[] = "PrefetchRetry";
constexpr char kKeyCacheStatsMetricName[] = "KeyCacheStats";
constexpr char kKeyCacheHit[] = "CacheHit";
constexpr char kKeyCacheMiss[] = "CacheMiss";
constexpr size_t kNetworkLatencyBufferInNanoseconds =
    duration_cast<nanoseconds>(seconds(60)).count();  // 60 seconds
// Fetching keys that were active 12 hours earlier and caching them helps
// prevent latency issues between the client and server.
// For keyset with backfill days configured, the start time will be further
// adjusted back by the backfill days.
constexpr size_t kPrefetchActiveHMACKeysStartTimeSubDiffInHours = 12;

// To prevent a gap in key rotation, prefetch active HMAC keys seven days in
// advance. Since the auto-refresh window is less than 7 days, pre-fetching
// the key within 7 days ensures that any pending active keys will be fetched in
// advance.
constexpr size_t kPrefetchActiveHMACKeysEndTimeAddedDiffInHours = 7 * 24;

ListActiveEncryptionKeysRequest GetListActiveKeysRequest(
    const std::vector<
        google::cmrt::sdk::private_key_service::v1::PrivateKeyEndpoint>&
        private_key_endpoints,
    const std::string& keyset_name, int keyset_backfill_days) {
  ListActiveEncryptionKeysRequest request;
  for (const auto& private_key_endpoint : private_key_endpoints) {
    auto* coordinator = request.add_key_endpoints();
    coordinator->set_endpoint(private_key_endpoint.endpoint());
    coordinator->set_gcp_wip_provider(private_key_endpoint.gcp_wip_provider());
    coordinator->set_gcp_cloud_function_url(
        private_key_endpoint.gcp_cloud_function_url());
  }
  request.set_key_set_name(keyset_name);

  auto now = TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();

  // Adjusting the start time to account for backfill key retrieval.
  auto key_fetching_query_start_time =
      keyset_backfill_days > 0
          ? now - duration_cast<nanoseconds>(
                      hours(kPrefetchActiveHMACKeysStartTimeSubDiffInHours +
                            keyset_backfill_days * 24))
                      .count()
          : now - duration_cast<nanoseconds>(
                      hours(kPrefetchActiveHMACKeysStartTimeSubDiffInHours))
                      .count();
  auto key_fetching_query_end_time =
      now + duration_cast<nanoseconds>(
                hours(kPrefetchActiveHMACKeysEndTimeAddedDiffInHours))
                .count();
  *request.mutable_query_time_range()->mutable_start_time() =
      TimeUtil::NanosecondsToTimestamp(key_fetching_query_start_time);
  *request.mutable_query_time_range()->mutable_end_time() =
      TimeUtil::NanosecondsToTimestamp(key_fetching_query_end_time);
  return request;
}

GetKeysetMetadataRequest CreateKeysetMetadataRequest(
    const std::vector<
        google::cmrt::sdk::private_key_service::v1::PrivateKeyEndpoint>&
        private_key_endpoints,
    const std::string& keyset_name) {
  GetKeysetMetadataRequest get_keyset_metadata_request;
  get_keyset_metadata_request.set_keyset_name(keyset_name);
  for (const auto& private_key_endpoint : private_key_endpoints) {
    // Any coordinator info can be used to fetch the keyset metadata.
    get_keyset_metadata_request.set_private_key_endpoint(
        private_key_endpoint.endpoint());
    return get_keyset_metadata_request;
  }
  return get_keyset_metadata_request;
}
}  // namespace

AutoRefreshKeyFetcherWithCache::AutoRefreshKeyFetcherWithCache(
    PrivateKeyClientInterface& key_client,
    const std::vector<
        google::cmrt::sdk::private_key_service::v1::PrivateKeyEndpoint>&
        private_key_endpoints,
    const std::string& keyset_name,
    DualWritingMetricClientInterface& metric_client,
    KeyFetcherOptions key_fetcher_options, const string& metric_namespace)
    : key_client_(key_client),
      keyset_name_(keyset_name),
      private_key_endpoints_(private_key_endpoints),
      refresh_duration_in_seconds_(
          key_fetcher_options.auto_refresh_time_duration.count()),
      metric_client_(metric_client),
      key_fetcher_options_(std::move(key_fetcher_options)),
      key_fetching_status_(FetchingStatus::FINISHED) {
  auto common_metric_labels =
      MetricUtils::CreateMetricLabelsWithComponentSignature(
          kAutoRefreshKeyFetcherWithCacheComponentName, keyset_name_);
  auto key_fetching_request_metric_info = MetricDefinition(
      kKeyFetchingRequestMetricsName, MetricUnit::METRIC_UNIT_COUNT,
      metric_namespace, common_metric_labels);
  key_fetching_request_metric_ = metric_client_.CreateAggregateMetric(
      std::move(key_fetching_request_metric_info));

  auto key_fetching_error_metric_info = MetricDefinition(
      kKeyFetchingErrorMetricsName, MetricUnit::METRIC_UNIT_COUNT,
      metric_namespace, common_metric_labels);
  key_fetching_error_metric_ = metric_client_.CreateAggregateMetric(
      std::move(key_fetching_error_metric_info));

  auto key_fetching_latency_metric_info = MetricDefinition(
      kKeyFetchingLegacyLatencyMetricName, MetricUnit::METRIC_UNIT_MILLISECONDS,
      metric_namespace, common_metric_labels);
  key_fetching_latency_metric_ = metric_client_.CreateTimeAggregateMetric(
      std::move(key_fetching_latency_metric_info));
  auto prefetch_retry_count_metric_info = MetricDefinition(
      kPrefetchRetryCountMetricName, MetricUnit::METRIC_UNIT_COUNT,
      metric_namespace, common_metric_labels);
  prefetch_retry_metric_ = metric_client_.CreateAggregateMetric(
      std::move(prefetch_retry_count_metric_info));

  auto key_cache_stats_metric_info =
      MetricDefinition(kKeyCacheStatsMetricName, MetricUnit::METRIC_UNIT_COUNT,
                       metric_namespace, common_metric_labels);
  key_cache_stats_metric_ = metric_client_.CreateAggregateMetric(
      std::move(key_cache_stats_metric_info), {kKeyCacheHit, kKeyCacheMiss});
}

core::ExecutionResult AutoRefreshKeyFetcherWithCache::Init() noexcept {
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.InitMetric(kKeyFetchingRequestMetricsName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to init key_fetching_request_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.InitMetric(kKeyFetchingErrorMetricsName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to init key_fetching_error_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.InitMetric(kKeyFetchingLegacyLatencyMetricName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to init key_fetching_latency_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.InitMetric(kPrefetchRetryCountMetricName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to init prefetch_retry_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.InitMetric(kKeyCacheStatsMetricName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to init key_cache_stats_metric_.");

  return SuccessExecutionResult();
}

core::ExecutionResult AutoRefreshKeyFetcherWithCache::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.RunMetric(kKeyFetchingRequestMetricsName,
                               kAutoRefreshKeyFetcherWithCacheComponentName,
                               keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Run key_fetching_request_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.RunMetric(kKeyFetchingErrorMetricsName,
                               kAutoRefreshKeyFetcherWithCacheComponentName,
                               keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Run key_fetching_error_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.RunMetric(kKeyFetchingLegacyLatencyMetricName,
                               kAutoRefreshKeyFetcherWithCacheComponentName,
                               keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Run key_fetching_latency_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.RunMetric(kPrefetchRetryCountMetricName,
                               kAutoRefreshKeyFetcherWithCacheComponentName,
                               keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Run prefetch_retry_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.RunMetric(kKeyCacheStatsMetricName,
                               kAutoRefreshKeyFetcherWithCacheComponentName,
                               keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Run key_cache_stats_metric_.");

  // Try to fetch keyset metadata for all SID keysets as KS confirmed that all
  // keysets are ready.
  FetchAndCacheKeysetMetadataRemote(KeyFetchingType::kPrefetch);

  // Wait synchronously for the first population to complete.
  // Sleep for a random delay to prevent multiple servers from prefetching at
  // once.
  static random_device random_device_local;
  static mt19937 random_generator(random_device_local());
  uniform_int_distribution<uint64_t> distribution;
  const auto max_delay_ms = key_fetcher_options_.max_prefetch_wait_time_millis;
  auto sleep_duration =
      milliseconds(distribution(random_generator) % max_delay_ms);
  sleep_for(sleep_duration);
  auto result = TryFetchAndCacheKeys(KeyFetchingType::kPrefetch).result();
  if (!result.Successful()) {
    if (key_fetcher_options_.prefetch_retry) {
      SCP_ERROR(kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid, result,
                "Fetching keys failed. Will retry fetching the keys after some "
                "delay.");
      sleep_duration =
          milliseconds(distribution(random_generator) % max_delay_ms);
      sleep_for(sleep_duration);
      LOG_IF_FAILURE(
          metric_client_.PutMetric(prefetch_retry_metric_.Increment()),
          kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
          "Failed to put metric");
      LOG_IF_FAILURE(
          TryFetchAndCacheKeys(KeyFetchingType::kPrefetchRetry).result(),
          kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
          "Refreshing keys failed. Will attempt to acquire new keys in %d "
          "seconds",
          refresh_duration_in_seconds_);
    } else {
      SCP_ERROR(
          kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid, result,
          "Refreshing keys failed. Will attempt to acquire new keys in %d "
          "seconds",
          refresh_duration_in_seconds_);
    }
  }
  refresh_thread_ = std::thread(
      &AutoRefreshKeyFetcherWithCache::RefreshThreadLoopFunction, this);
  return SuccessExecutionResult();
}

core::ExecutionResult AutoRefreshKeyFetcherWithCache::Stop() noexcept {
  stop_notification_.Notify();
  if (refresh_thread_.joinable()) {
    refresh_thread_.join();
  }

  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.StopMetric(kKeyFetchingRequestMetricsName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Stop key_fetching_request_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.StopMetric(kKeyFetchingErrorMetricsName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Stop key_fetching_error_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.StopMetric(kKeyFetchingLegacyLatencyMetricName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Stop key_fetching_latency_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.StopMetric(kPrefetchRetryCountMetricName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Stop prefetch_retry_metric_.");
  RETURN_AND_LOG_IF_FAILURE(
      metric_client_.StopMetric(kKeyCacheStatsMetricName,
                                kAutoRefreshKeyFetcherWithCacheComponentName,
                                keyset_name_),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to Stop key_cache_stats_metric_.");
  return SuccessExecutionResult();
}

core::ExecutionResultOr<ListActiveEncryptionKeysResponse>
AutoRefreshKeyFetcherWithCache::FetchKeysFromRemoteWithActiveKeysApi(
    absl::string_view key_fetching_type) noexcept {
  LOG_IF_FAILURE(
      metric_client_.PutMetric(key_fetching_request_metric_.Increment()),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to put metric");

  // Log new key fetching request metric using OpenTelemetry
  PushKeyFetchingRequestMetric(metric_client_, KeyType::kSidKey,
                               key_fetching_type, keyset_name_);

  ListActiveEncryptionKeysRequest list_active_keys_request =
      GetListActiveKeysRequest(private_key_endpoints_, keyset_name_,
                               keyset_backfill_days_.load());

  auto fetching_start_time_in_ns =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
  auto response_or =
      key_client_.ListActiveEncryptionKeysSync(list_active_keys_request);
  auto fetching_end_time_in_ns =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
  auto latency_in_millis =
      (fetching_end_time_in_ns - fetching_start_time_in_ns) / 1000000.0;
  LOG_IF_FAILURE(
      metric_client_.PutMetric(key_fetching_latency_metric_.WithValue(
          absl::StrCat(latency_in_millis))),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to put metric");

  // Log new key fetching latency metric using OpenTelemetry
  PushKeyFetchingLatencyMetric(metric_client_, KeyType::kSidKey,
                               key_fetching_type, keyset_name_,
                               latency_in_millis);

  auto fetching_result = response_or.result();
  if (fetching_result.Successful()) {
    if (response_or->private_keys_size() > 0) {
      return std::move(*response_or);
    }
    fetching_result =
        FailureExecutionResult(SC_CPIO_KEY_FETCHER_NO_KEY_FETCHED);
  }

  SCP_ERROR(kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
            fetching_result, "SID key refresh failed for keyset %s",
            keyset_name_.c_str());

  LOG_IF_FAILURE(
      metric_client_.PutMetric(key_fetching_error_metric_.Increment()),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to put metric");

  // Log new key fetching error metric using OpenTelemetry
  PushKeyFetchingErrorMetric(
      metric_client_, KeyType::kSidKey, key_fetching_type, keyset_name_,
      MapToKeyFetchingErrorString(fetching_result.status_code));

  return fetching_result;
}

void AutoRefreshKeyFetcherWithCache::CacheKeys(
    const vector<Key>& keys_list) noexcept {
  // Skips the key cache refresh if the response is empty.
  if (keys_list.empty()) {
    return;
  }

  std::unique_lock lock(key_cache_mutex_);
  // Clear the cache and repopulate with the latest set of keys.
  key_cache_map_.clear();
  for (auto& key : keys_list) {
    if (!key_cache_map_.insert(make_pair(key.expiration_timestamp, key))
             .second) {
      // Key insert failed.
      SCP_ERROR(
          kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
          FailureExecutionResult(SC_CPIO_KEY_FETCHER_CACHE_INSERT_FAILURE),
          "Insert key %s into cache btree_map failed", key.key_id.c_str());
    }
  }
}

core::ExecutionResultOr<vector<Key>>
AutoRefreshKeyFetcherWithCache::GetKeysFromCache(
    core::Timestamp timestamp_ns) noexcept {
  std::shared_lock lock(key_cache_mutex_);
  vector<Key> keys_to_return;
  // Finds the first key with an expiration_time that is not less than
  // `timestamp_ns`
  auto key_it = key_cache_map_.lower_bound(timestamp_ns);
  while (key_it != key_cache_map_.end()) {
    if (key_it->second.expiration_timestamp > timestamp_ns &&
        key_it->second.activation_timestamp <= timestamp_ns) {
      keys_to_return.push_back(key_it->second);
    }
    key_it++;
  }

  if (keys_to_return.empty()) {
    auto failure = FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND);
    SCP_ERROR(kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid, failure,
              "No active key return for keyset %s", keyset_name_.c_str());
    return failure;
  }

  return keys_to_return;
}

core::ExecutionResult
AutoRefreshKeyFetcherWithCache::ValidateKeySelectionTimestamp(
    core::Timestamp key_selection_timestamp_ns) noexcept {
  auto now_ns = TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();
  // The key selection timestamp is valid if it is within
  // [now - kNetworkLatencyBufferInNanoseconds, now +
  // kNetworkLatencyBufferInNanoseconds].
  if (key_selection_timestamp_ns <=
          now_ns + kNetworkLatencyBufferInNanoseconds &&
      key_selection_timestamp_ns >=
          now_ns - kNetworkLatencyBufferInNanoseconds) {
    return SuccessExecutionResult();
  }

  // Try to fetch the keyset metadata when keyset_backfill_days_ isn't being
  // cached.
  if (keyset_backfill_days_.load() < 0) {
    FetchAndCacheKeysetMetadataRemote(KeyFetchingType::kOnDemand);
  }

  // Keyset with backfill days allows key selection timestamp to be
  // backfilled.
  if (keyset_backfill_days_.load() > 0) {
    auto backfill_duration_in_ns =
        duration_cast<nanoseconds>(hours(24 * keyset_backfill_days_.load()))
            .count();
    // The key selection timestamp is valid if it is within
    // [now - backfill_duration_in_ns - kNetworkLatencyBufferInNanoseconds,
    // now + kNetworkLatencyBufferInNanoseconds].
    if (key_selection_timestamp_ns <=
            now_ns + kNetworkLatencyBufferInNanoseconds &&
        key_selection_timestamp_ns >= now_ns - backfill_duration_in_ns -
                                          kNetworkLatencyBufferInNanoseconds) {
      return SuccessExecutionResult();
    }
  }

  return FailureExecutionResult(
      SC_CPIO_KEY_FETCHER_INVALID_KEY_SELECTION_TIMESTAMP);
}

core::ExecutionResultOr<vector<Key>>
AutoRefreshKeyFetcherWithCache::GetValidKeys(
    core::Timestamp key_selection_timestamp_ns) noexcept {
  if (key_fetcher_options_.enable_key_selection_timestamp_validation) {
    RETURN_AND_LOG_IF_FAILURE(
        ValidateKeySelectionTimestamp(key_selection_timestamp_ns),
        kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
        "Key selection timestamp %lld validation failed for keyset %s with "
        "backfill %d",
        key_selection_timestamp_ns, keyset_name_.c_str(),
        keyset_backfill_days_.load());
  }

  auto keys_in_cache = GetKeysFromCache(key_selection_timestamp_ns);
  LOG_IF_FAILURE(
      metric_client_.PutMetric(key_cache_stats_metric_.Increment(
          1, keys_in_cache.Successful() ? kKeyCacheHit : kKeyCacheMiss)),
      kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
      "Failed to put metric");

  // Log new key cache stats metric using OpenTelemetry
  PushKeyCacheStatusMetric(metric_client_, KeyType::kSidKey, keyset_name_,
                           keys_in_cache.Successful()
                               ? KeyCacheStatus::kValidKeyCacheHit
                               : KeyCacheStatus::kValidKeyCacheMiss);

  if (keys_in_cache.Successful() || !OnDemandFetchingEnabled()) {
    return keys_in_cache;
  }

  if (!FetchingInProgress()) {
    // This is to double confirm there is no thread finished key fetching and
    // key caching but the status is not marked FINISHED yet.
    keys_in_cache = GetKeysFromCache(key_selection_timestamp_ns);
    if (keys_in_cache.Successful()) {
      return keys_in_cache;
    }

    if (MarkFetchingInProgress()) {
      // Fetching result is ignored.
      auto keys_or = TryFetchAndCacheKeys(KeyFetchingType::kOnDemand);
      MarkFetchingFinished();
      return GetKeysFromCache(key_selection_timestamp_ns);
    }
  }

  WaitForKeyReady();
  // If the key fetching thread failed to fetch keys, all waiting thread will
  // return empty keys. When another batch of key fetching requests come, key
  // will be re-fetched from remote. This is OK since:
  // 1. CPIO PrivateKeyClient will retry the key fetching (exponential backoff
  // for ~6min) and key decryption (exponential backoff for ~5min). Retry
  // immediately here will not help.
  // 2. Key fetching failure here will cause the whole request failure and
  // return retryable error and retried outside.
  return GetKeysFromCache(key_selection_timestamp_ns);
}

core::ExecutionResultOr<vector<Key>>
AutoRefreshKeyFetcherWithCache::TryFetchAndCacheKeys(
    absl::string_view key_fetching_type) noexcept {
  vector<Key> keys;
  auto response_or = FetchKeysFromRemoteWithActiveKeysApi(key_fetching_type);
  RETURN_IF_FAILURE(response_or.result());
  keys = google::scp::cpio::ExtractKeys(response_or->private_keys());
  CacheKeys(keys);

  return keys;
}

void AutoRefreshKeyFetcherWithCache::FetchAndCacheKeysetMetadataRemote(
    absl::string_view key_fetching_type) noexcept {
  GetKeysetMetadataRequest get_keyset_metadata_request =
      CreateKeysetMetadataRequest(private_key_endpoints_, keyset_name_);
  // Log keyset fetching request metric using OpenTelemetry
  PushKeyFetchingRequestMetric(metric_client_, KeyType::kKeysetMetadata,
                               key_fetching_type, keyset_name_);

  if (auto response_or =
          key_client_.GetKeysetMetadataSync(get_keyset_metadata_request);
      response_or.Successful()) {
    keyset_active_key_count_.store(response_or->active_key_count());
    keyset_backfill_days_.store(response_or->backfill_days());
  } else {
    // Log keyset fetching error metric using OpenTelemetry
    PushKeyFetchingErrorMetric(
        metric_client_, KeyType::kKeysetMetadata, key_fetching_type,
        keyset_name_,
        MapToKeyFetchingErrorString(response_or.result().status_code));
    SCP_ERROR(kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
              response_or.result(),
              "SID keyset metadata fetching failed for keyset %s",
              keyset_name_.c_str());
  }
}

bool AutoRefreshKeyFetcherWithCache::OnDemandFetchingEnabled() noexcept {
  return key_fetcher_options_.enable_on_demand_fetching_for_hmac_key;
}

void AutoRefreshKeyFetcherWithCache::MarkFetchingFinished() noexcept {
  std::unique_lock lock(status_mutex_);
  key_fetching_status_ = FetchingStatus::FINISHED;
  lock.unlock();
  status_cv_.notify_all();
}

bool AutoRefreshKeyFetcherWithCache::MarkFetchingInProgress() noexcept {
  std::unique_lock lock(status_mutex_);
  if (key_fetching_status_ == FetchingStatus::IN_PROGRESS) {
    lock.unlock();
    return false;
  } else {
    key_fetching_status_ = FetchingStatus::IN_PROGRESS;
    lock.unlock();
    return true;
  }
}

void AutoRefreshKeyFetcherWithCache::WaitForKeyReady() noexcept {
  std::shared_lock lock(status_mutex_);
  // When the shared_lock can be acquired, it means no unique_lock is on it,
  // which indicates: either the status is already changed to FINISHED or the
  // other thread is going to acquire the unique_lock and change the status.
  // So adding a check before wait can make sure the condition wait is always
  // setted up before a unique_lock is acquired and notification is sent.
  if (key_fetching_status_ != FetchingStatus::IN_PROGRESS) {
    return;
  }
  // wait_for will unlock the lock automatically.
  status_cv_.wait_for(
      lock, key_fetcher_options_.on_demand_fetching_waiting_timeout,
      [this] { return key_fetching_status_ != FetchingStatus::IN_PROGRESS; });
}

bool AutoRefreshKeyFetcherWithCache::FetchingInProgress() noexcept {
  bool in_progress = false;
  std::shared_lock lock(status_mutex_);
  in_progress = key_fetching_status_ == FetchingStatus::IN_PROGRESS;
  lock.unlock();
  return in_progress;
}

bool AutoRefreshKeyFetcherWithCache::HasEnoughValidKeyCached() noexcept {
  auto check_timestamp =
      TimeProvider::GetWallTimestampInNanosecondsAsClockTicks() +
      duration_cast<nanoseconds>(
          hours(key_fetcher_options_.auto_refresh_key_cache_valid_in_days * 24))
          .count();
  auto keys_in_cache_or = GetKeysFromCache(check_timestamp);
  if (keys_in_cache_or.Successful() &&
      keys_in_cache_or->size() == keyset_active_key_count_.load()) {
    return true;
  } else {
    SCP_ERROR(kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
              keys_in_cache_or.result(),
              "Auto-refresh checking: the number of active keys in cache "
              "doesn't match the keyset "
              "metadata for keyset %s. Cached active key count: %d, "
              "Keyset metadata active key count: %d",
              keyset_name_.c_str(),
              keys_in_cache_or.Successful() ? keys_in_cache_or->size() : -1,
              keyset_active_key_count_.load());
    return false;
  }
}

void AutoRefreshKeyFetcherWithCache::RefreshThreadLoopFunction() noexcept {
  while (!stop_notification_.WaitForNotificationWithTimeout(
      absl::Seconds(refresh_duration_in_seconds_))) {
    // Try to fetch the keyset metadata when keyset_active_key_count_ isn't
    // being cached.
    if (keyset_active_key_count_.load() == 0) {
      FetchAndCacheKeysetMetadataRemote(KeyFetchingType::kAutoRefresh);
    }

    // Skips the key fetching if the cache is valid in schedule checking time.
    if (keyset_active_key_count_.load() > 0 && HasEnoughValidKeyCached()) {
      continue;
    }

    SCP_DEBUG(kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
              "Refreshing keys for keyset %s", keyset_name_.c_str());

    LOG_IF_FAILURE(TryFetchAndCacheKeys(KeyFetchingType::kAutoRefresh).result(),
                   kAutoRefreshKeyFetcherWithCacheComponentName, kZeroUuid,
                   "Cannot refresh the cache for keyset %s.",
                   keyset_name_.c_str());
  }
}

string AutoRefreshKeyFetcherWithCache::MapToKeyFetchingErrorString(
    core::StatusCode status_code) noexcept {
  if (status_code == SC_CPIO_KEY_NOT_FOUND ||
      status_code == SC_CPIO_ENTITY_NOT_FOUND ||
      status_code == SC_CPIO_INVALID_ARGUMENT) {
    return KeyFetchingErrorType::kInvalidKeyId;
  }
  return KeyFetchingErrorType::kGenericError;
}
}  // namespace google::scp::cpio
