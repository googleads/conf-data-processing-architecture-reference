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

#include "coordinator_key_fetcher_with_cache_base.h"

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_join.h"
#include "core/common/global_logger/src/global_logger.h"
#include "google/protobuf/util/time_util.h"
#include "public/core/interface/execution_result_macros.h"
#include "public/cpio/interface/error_codes.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"

#include "error_codes.h"
#include "key_fetching_utils.h"

using google::cmrt::sdk::private_key_service::v1::
    ListActiveEncryptionKeysRequest;
using google::cmrt::sdk::private_key_service::v1::
    ListActiveEncryptionKeysResponse;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::cmrt::sdk::v1::KeyCoordinatorConfiguration;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::SC_CPIO_ENTITY_NOT_FOUND;
using google::scp::core::errors::SC_CPIO_INVALID_ARGUMENT;
using google::scp::core::errors::SC_CPIO_KEY_COUNT_MISMATCH;
using google::scp::core::errors::SC_CPIO_KEY_FETCHER_FETCHING_TIMEOUT;
using google::scp::core::errors::SC_CPIO_KEY_NOT_FOUND;
using google::scp::cpio::KeyFetchingType;
using std::mt19937;
using std::pair;
using std::random_device;
using std::shared_ptr;
using std::string;
using std::uniform_int_distribution;
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
constexpr milliseconds kThreadSleepIntervalForKeyReady = milliseconds(5);

// Non-retryable key fetching errors
const absl::flat_hash_set<StatusCode> kUnretryableKeyFetchingErrors = {
    SC_CPIO_KEY_NOT_FOUND, SC_CPIO_KEY_COUNT_MISMATCH, SC_CPIO_ENTITY_NOT_FOUND,
    SC_CPIO_INVALID_ARGUMENT};

uint64_t GetKeyCacheLifetimeSeconds(
    const KeyFetcherOptions& key_fetcher_options) {
  return key_fetcher_options.key_cache_lifetime.count();
}

uint64_t GetFetchingFailureCacheLifetimeSeconds(
    const KeyFetcherOptions& key_fetcher_options) {
  return key_fetcher_options.fetching_failure_cache_lifetime.count();
}

ListPrivateKeysRequest GetListPrivateKeysRequestBase(
    const KeyCoordinatorConfiguration& key_service_options) {
  ListPrivateKeysRequest request;
  for (const auto& coordinator_info :
       key_service_options.private_key_endpoints()) {
    auto* coordinator = request.add_key_endpoints();
    coordinator->set_endpoint(coordinator_info.endpoint());
    coordinator->set_gcp_wip_provider(coordinator_info.gcp_wip_provider());
    coordinator->set_gcp_cloud_function_url(
        coordinator_info.gcp_cloud_function_url());
  }
  return request;
}

ListActiveEncryptionKeysRequest GetListActiveKeysRequestBase(
    const KeyCoordinatorConfiguration& key_service_options) {
  ListActiveEncryptionKeysRequest request;
  for (const auto& coordinator_info :
       key_service_options.private_key_endpoints()) {
    auto* coordinator = request.add_key_endpoints();
    coordinator->set_endpoint(coordinator_info.endpoint());
    coordinator->set_gcp_wip_provider(coordinator_info.gcp_wip_provider());
    coordinator->set_gcp_cloud_function_url(
        coordinator_info.gcp_cloud_function_url());
  }
  return request;
}

}  // namespace

CoordinatorKeyFetcherWithCacheBase::CoordinatorKeyFetcherWithCacheBase(
    shared_ptr<AsyncExecutorInterface>& async_executor,
    PrivateKeyClientInterface& key_client,
    DualWritingMetricClientInterface& metric_client,
    const KeyCoordinatorConfiguration& key_service_options,
    KeyFetcherOptions key_fetcher_options, absl::string_view component_name,
    absl::string_view key_type, const std::string& metric_namespace)
    : key_cache_(
          GetKeyCacheLifetimeSeconds(key_fetcher_options),
          true /* extend_entry_lifetime_on_access */,
          true /* block_entry_while_eviction */,
          [](auto&, auto&, auto should_delete_entry) {
            should_delete_entry(true);
          } /*function on before garbage collection*/,
          async_executor),
      fetching_failure_cache_(
          GetFetchingFailureCacheLifetimeSeconds(key_fetcher_options),
          false /* extend_entry_lifetime_on_access */,
          true /* block_entry_while_eviction */,
          [](auto&, auto&, auto should_delete_entry) {
            should_delete_entry(true);
          } /*function on before garbage collection*/,
          async_executor, key_fetcher_options.use_read_lock_for_cache_read),
      key_client_(key_client),
      list_private_keys_request_base_(
          GetListPrivateKeysRequestBase(key_service_options)),
      list_active_keys_request_base_(
          GetListActiveKeysRequestBase(key_service_options)),
      allowed_keysets_list_(key_service_options.key_namespace().begin(),
                            key_service_options.key_namespace().end()),
      key_fetcher_options_(std::move(key_fetcher_options)),
      metric_client_(metric_client),
      component_name_(component_name),
      key_type_(key_type) {
  allowed_keysets_name_ = absl::StrJoin(allowed_keysets_list_, "_");
}

ExecutionResult CoordinatorKeyFetcherWithCacheBase::Init() noexcept {
  RETURN_AND_LOG_IF_FAILURE(key_cache_.Init(), component_name_, kZeroUuid,
                            "Failed to init key_cache_.");
  RETURN_AND_LOG_IF_FAILURE(fetching_failure_cache_.Init(), component_name_,
                            kZeroUuid,
                            "Failed to init fetching_failure_cache_.");
  return SuccessExecutionResult();
}

ExecutionResult CoordinatorKeyFetcherWithCacheBase::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(key_cache_.Run(), component_name_, kZeroUuid,
                            "Failed to run key_cache_.");
  RETURN_AND_LOG_IF_FAILURE(fetching_failure_cache_.Run(), component_name_,
                            kZeroUuid,
                            "Failed to run fetching_failure_cache_.");

  if (key_fetcher_options_.prefetch_keys) {
    PrefetchKeys();
  }

  return SuccessExecutionResult();
}

ExecutionResult CoordinatorKeyFetcherWithCacheBase::Stop() noexcept {
  RETURN_AND_LOG_IF_FAILURE(key_cache_.Stop(), component_name_, kZeroUuid,
                            "Failed to stop key_cache_.");
  RETURN_AND_LOG_IF_FAILURE(fetching_failure_cache_.Stop(), component_name_,
                            kZeroUuid,
                            "Failed to stop fetching_failure_cache_.");
  return SuccessExecutionResult();
}

core::ExecutionResultOr<ListPrivateKeysResponse>
CoordinatorKeyFetcherWithCacheBase::FetchKeysFromRemote(
    const ListPrivateKeysRequest& request, absl::string_view key_fetching_type,
    absl::string_view keyset_name) {
  PushKeyFetchingRequestMetric(metric_client_, key_type_,
                               key_fetching_type, keyset_name);
  auto fetching_start_time_in_ns =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();

  auto response_or = key_client_.ListPrivateKeysSync(request);

  auto fetching_end_time_in_ns =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
  auto latency_in_millis =
      duration_cast<milliseconds>(
          nanoseconds(fetching_end_time_in_ns - fetching_start_time_in_ns))
          .count();
  PushKeyFetchingLatencyMetric(metric_client_, key_type_,
                               key_fetching_type, keyset_name,
                               latency_in_millis);

  if (!response_or.Successful()) {
    PushKeyFetchingErrorMetric(
        metric_client_, key_type_, key_fetching_type, keyset_name,
        MapToKeyFetchingErrorString(response_or.result().status_code));
  }
  return response_or;
}

core::ExecutionResultOr<ListActiveEncryptionKeysResponse>
CoordinatorKeyFetcherWithCacheBase::FetchKeysFromRemoteWithActiveKeysApi(
    const ListActiveEncryptionKeysRequest& request,
    absl::string_view key_fetching_type, absl::string_view keyset_name) {
  PushKeyFetchingRequestMetric(metric_client_, key_type_,
                               key_fetching_type, keyset_name);
  auto fetching_start_time_in_ns =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();

  auto response_or = key_client_.ListActiveEncryptionKeysSync(request);

  auto fetching_end_time_in_ns =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
  auto latency_in_millis =
      duration_cast<milliseconds>(
          nanoseconds(fetching_end_time_in_ns - fetching_start_time_in_ns))
          .count();
  PushKeyFetchingLatencyMetric(metric_client_, key_type_,
                               key_fetching_type, keyset_name,
                               latency_in_millis);

  if (!response_or.Successful()) {
    PushKeyFetchingErrorMetric(
        metric_client_, key_type_, key_fetching_type, keyset_name,
        MapToKeyFetchingErrorString(response_or.result().status_code));
  }
  return response_or;
}

void CoordinatorKeyFetcherWithCacheBase::SleepRandomDuration() noexcept {
  static random_device random_device_local;
  static mt19937 random_generator(random_device_local());
  uniform_int_distribution<uint64_t> distribution;
  auto max_delay_ms = key_fetcher_options_.max_prefetch_wait_time_millis;
  sleep_for(milliseconds(distribution(random_generator) % max_delay_ms));
}

void CoordinatorKeyFetcherWithCacheBase::PrefetchKeys() noexcept {
  // Sleep for a random delay to prevent multiple servers from prefetching at
  // once.
  SleepRandomDuration();
  auto one_week = duration_cast<nanoseconds>(hours(24 * 7)).count();
  for (const auto& keyset_name : allowed_keysets_list_) {
    auto now = TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();
    auto prefetch_config_iterator =
        key_fetcher_options_.encryption_key_prefetch_config_map.find(
            keyset_name);

    // New Encryption Key Prefetching Configuration
    if (prefetch_config_iterator !=
        key_fetcher_options_.encryption_key_prefetch_config_map.end()) {
      const auto& keyset_config = prefetch_config_iterator->second;

      if (keyset_config.key_ids_size() > 0) {
        PrefetchWithListPrivateKeys(keyset_name, keyset_config.key_ids());
      }
      if (keyset_config.has_prefetch_duration()) {
        // ListActiveKeys needs to specify its start and end time
        // Start time using new prefetch config is (now - duration)
        // End time using new prefetch config is (now + 1 week)
        // We will prefetch for one future week by default to ensure reasonable
        // future keys are in the cache.
        auto duration_nanos = keyset_config.prefetch_duration().nanos();
        auto duration_seconds = keyset_config.prefetch_duration().seconds();
        int64_t duration =
            duration_cast<nanoseconds>(seconds(duration_seconds) +
                                       nanoseconds(duration_nanos))
                .count();
        auto start_time = TimeUtil::NanosecondsToTimestamp(now - duration);
        auto end_time = TimeUtil::NanosecondsToTimestamp(now + one_week);
        PrefetchWithListActiveKeys(keyset_name, start_time, end_time);
      }
    } else {
      // ListActiveKeys needs to specify its start and end time to ensure the
      // behaviours for ListPrivateKeys and ListActiveKeys are in sync in the
      // legacy prefetch. Start time for the old prefetch system is (now - age
      // + 1 week) End time for the old prefetch system is (now + 1 week)
      int64_t age =
          duration_cast<nanoseconds>(key_fetcher_options_.prefetch_keys_max_age)
              .count();
      auto start_time = TimeUtil::NanosecondsToTimestamp(now - age + one_week);
      auto end_time = TimeUtil::NanosecondsToTimestamp(now + one_week);
      PrefetchWithListActiveKeys(keyset_name, start_time, end_time);
    }
  }
}

void CoordinatorKeyFetcherWithCacheBase::PrefetchWithListPrivateKeys(
    const std::string& keyset_name,
    const std::optional<google::protobuf::RepeatedPtrField<std::string>>&
        key_ids) noexcept {
  // ListPrivateKeys accepts age as a duration based on its creation time,
  // and the activation time is 1 week after the creation time.
  ListPrivateKeysRequest prefetch_request = list_private_keys_request_base_;
  prefetch_request.set_key_set_name(keyset_name);
  if (key_ids.has_value()) {
    for (const auto& key_id : *key_ids) {
      prefetch_request.add_key_ids(key_id);
    }
  } else {
    prefetch_request.set_max_age_seconds(
        key_fetcher_options_.prefetch_keys_max_age.count());
  }
  SCP_INFO(component_name_, kZeroUuid,
           "Prefetching with ListPrivateKeys for for keyset %s.",
           keyset_name.c_str());
  auto response_or = FetchKeysFromRemote(
      prefetch_request, KeyFetchingType::kPrefetch, keyset_name);

  if (!response_or.Successful() && key_fetcher_options_.prefetch_retry) {
    SleepRandomDuration();
    SCP_INFO(component_name_, kZeroUuid, "Retrying prefetching for keyset %s",
             keyset_name.c_str());
    response_or = FetchKeysFromRemote(
        prefetch_request, KeyFetchingType::kPrefetchRetry, keyset_name);
  }
  if (response_or.Successful()) {
    CacheValidKey(ExtractKeys(response_or->private_keys()));
  } else {
    SCP_ERROR(component_name_, kZeroUuid, response_or.result(),
              "ListPrivateKeys prefetching failed for keyset %s.",
              keyset_name.c_str());
  }
}

void CoordinatorKeyFetcherWithCacheBase::PrefetchWithListActiveKeys(
    const std::string& keyset_name,
    const google::protobuf::Timestamp& start_time,
    const google::protobuf::Timestamp& end_time) noexcept {
  ListActiveEncryptionKeysRequest prefetch_request =
      list_active_keys_request_base_;
  prefetch_request.set_key_set_name(keyset_name);
  *prefetch_request.mutable_query_time_range()->mutable_start_time() =
      start_time;
  *prefetch_request.mutable_query_time_range()->mutable_end_time() = end_time;
  SCP_INFO(component_name_, kZeroUuid,
           "Prefetching with ListActiveKeys for keyset %s.",
           keyset_name.c_str());

  auto response_or = FetchKeysFromRemoteWithActiveKeysApi(
      prefetch_request, KeyFetchingType::kPrefetch, keyset_name);

  if (!response_or.Successful() && key_fetcher_options_.prefetch_retry) {
    SleepRandomDuration();
    SCP_INFO(component_name_, kZeroUuid, "Retrying prefetching for keyset %s",
             keyset_name.c_str());
    response_or = FetchKeysFromRemoteWithActiveKeysApi(
        prefetch_request, KeyFetchingType::kPrefetchRetry, keyset_name);
  }
  if (response_or.Successful()) {
    CacheValidKey(ExtractKeys(response_or->private_keys()));
  } else {
    SCP_ERROR(component_name_, kZeroUuid, response_or.result(),
              "Active keys prefetching failed for keyset %s.",
              keyset_name.c_str());
  }
}

core::ExecutionResultOr<Key>
CoordinatorKeyFetcherWithCacheBase::FetchValidateAndCacheKey(
    const std::string& key_id) noexcept {
  ListPrivateKeysRequest request;
  request.CopyFrom(list_private_keys_request_base_);
  request.add_key_ids(key_id);

  // We don't know the exact keyset yet for most cases, so use the
  // allowed_keysets_name which may be a list for metric recording.
  auto response_or = FetchKeysFromRemote(request, KeyFetchingType::kOnDemand,
                                         allowed_keysets_name_);

  return ValidateAndCacheKey(key_id, response_or);
}

core::ExecutionResultOr<Key>
CoordinatorKeyFetcherWithCacheBase::ValidateAndCacheKey(
    const string key_id, const core::ExecutionResultOr<ListPrivateKeysResponse>&
                             list_keys_response_or) noexcept {
  auto fetching_result = list_keys_response_or.result();

  // The error metric for failed ListPrivateKeysResponse is already pushed. Here
  // we only push for validation failure.
  if (fetching_result.Successful() &&
      list_keys_response_or->private_keys().size() != 1) {
    fetching_result = list_keys_response_or->private_keys().size() == 0
                          ? FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)
                          : FailureExecutionResult(SC_CPIO_KEY_COUNT_MISMATCH);

    PushKeyFetchingErrorMetric(
        metric_client_, key_type_, KeyFetchingType::kOnDemand,
        allowed_keysets_name_,
        MapToKeyFetchingErrorString(fetching_result.status_code));
  }

  if (!fetching_result.Successful()) {
    SCP_ERROR(component_name_, kZeroUuid, fetching_result,
              "The key fetching failed for key ID %s", key_id.c_str());
    CacheFailureResult(key_id, fetching_result);
    return fetching_result;
  }

  auto& fetched_key = list_keys_response_or->private_keys(0);
  const auto& keyset_name = fetched_key.key_set_name();

  // Checks that the key belongs to the allowed key sets configured by the
  // application configuration.
  if (!keyset_name.empty() && allowed_keysets_list_.count(keyset_name) == 0) {
    auto failure = FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND);
    SCP_ERROR(component_name_, kZeroUuid, failure,
              "The key set %s for this key does not belong to "
              "the key sets allowed by the application",
              keyset_name.c_str());
    // Log new key fetching error metric using OpenTelemetry
    PushKeyFetchingErrorMetric(
        metric_client_, key_type_, KeyFetchingType::kOnDemand,
        allowed_keysets_name_,
        MapToKeyFetchingErrorString(failure.status_code));
    CacheFailureResult(fetched_key.key_id(), failure);
    return failure;
  }

  auto keys = ExtractKeys(list_keys_response_or->private_keys());
  auto current_time = TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();
  auto key_age_in_days =
      duration_cast<hours>(
          nanoseconds(current_time - keys[0].activation_timestamp))
          .count() /
      24;
  PushKeyAgeInDaysMetric(metric_client_, key_type_,
                         KeyFetchingType::kOnDemand, keyset_name,
                         key_age_in_days);

  SCP_INFO(component_name_, kZeroUuid,
           "OndemandFetchingKeyId: %s | KeysetName: %s | KeyAge: %d",
           key_id.c_str(), keyset_name.c_str(), key_age_in_days);
  CacheValidKey(keys);
  // Already checked the size is 1.
  return keys[0];
}

core::ExecutionResultOr<Key> CoordinatorKeyFetcherWithCacheBase::GetKey(
    const std::string& key_id) noexcept {
  auto key = GetKeyFromValidKeyCache(key_id);
  if (key.has_value()) {
    // Use allowed_keysets_name_ which might be a list to represent the keyset
    // because we don't have exact keyset_name available in the cache.
    PushKeyCacheStatusMetric(metric_client_, key_type_,
                             allowed_keysets_name_,
                             KeyCacheStatus::kValidKeyCacheHit);
    return key.value();
  }
  auto failure_result = GetFetchingFailureFromCache(key_id);
  // Only return directly when the failure is not retryable.
  if (failure_result.has_value() &&
      kUnretryableKeyFetchingErrors.contains(failure_result->status_code)) {
    PushKeyCacheStatusMetric(metric_client_, key_type_,
                             allowed_keysets_name_,
                             KeyCacheStatus::kInvalidKeyCacheHit);
    return failure_result.value();
  }

  PushKeyCacheStatusMetric(metric_client_, key_type_,
                           allowed_keysets_name_,
                           KeyCacheStatus::kValidKeyCacheMiss);

  if (!FetchingInProgress(key_id)) {
    // This is to double confirm there is no thread finished key fetching
    // and key caching but the in progress status is not updated yet.
    key = GetKeyFromValidKeyCache(key_id);
    if (key.has_value()) {
      return key.value();
    }
    failure_result = GetFetchingFailureFromCache(key_id);
    // Only return directly when the failure is not retryable.
    if (failure_result.has_value() &&
        kUnretryableKeyFetchingErrors.contains(failure_result->status_code)) {
      return failure_result.value();
    }

    // Failing to mark IN_PROGRESS status means some other thread is already
    // fetching the key. So it will fall to the WaitForKeyReady() process.
    if (MarkFetchingInProgress(key_id)) {
      auto fetched_key_or = FetchValidateAndCacheKey(key_id);
      MarkFetchingFinished(key_id);
      return fetched_key_or;
    }
  }

  WaitForKeyReady(key_id);

  key = GetKeyFromValidKeyCache(key_id);
  if (key.has_value()) {
    return key.value();
  }
  failure_result = GetFetchingFailureFromCache(key_id);
  // Another thread just finished key fetching and it is useless to retry
  // immediately, so we return directly.
  if (failure_result.has_value()) {
    return failure_result.value();
  }

  // It means the waiting timeout When this happens.
  auto timeout_failure =
      FailureExecutionResult(SC_CPIO_KEY_FETCHER_FETCHING_TIMEOUT);
  SCP_ERROR(component_name_, kZeroUuid, timeout_failure,
            "The key fetching failed for key %s.", key_id.c_str());
  return timeout_failure;
}

void CoordinatorKeyFetcherWithCacheBase::MarkFetchingFinished(
    const string& key_id) noexcept {
  std::unique_lock lock(in_progress_key_cache_mutex_);
  in_progress_key_cache_.erase(key_id);
  lock.unlock();
}

bool CoordinatorKeyFetcherWithCacheBase::MarkFetchingInProgress(
    const string& key_id) noexcept {
  std::unique_lock lock(in_progress_key_cache_mutex_);
  if (auto it = in_progress_key_cache_.find(key_id);
      it != in_progress_key_cache_.end()) {
    lock.unlock();
    return false;
  } else {
    in_progress_key_cache_.insert(key_id);
    lock.unlock();
    return true;
  }
}

void CoordinatorKeyFetcherWithCacheBase::WaitForKeyReady(
    const string& key_id) noexcept {
  auto start_time = system_clock::now();
  auto end_time = start_time;
  while (FetchingInProgress(key_id) &&
         (end_time - start_time) <
             key_fetcher_options_.on_demand_fetching_waiting_timeout) {
    sleep_for(kThreadSleepIntervalForKeyReady);
    end_time = system_clock::now();
  }
}

bool CoordinatorKeyFetcherWithCacheBase::FetchingInProgress(
    const string& key_id) noexcept {
  bool in_progress = false;
  std::shared_lock lock(in_progress_key_cache_mutex_);
  auto it = in_progress_key_cache_.find(key_id);
  in_progress = it != in_progress_key_cache_.end();
  lock.unlock();
  return in_progress;
}

string CoordinatorKeyFetcherWithCacheBase::MapToKeyFetchingErrorString(
    StatusCode status_code) noexcept {
  if (status_code == SC_CPIO_KEY_NOT_FOUND ||
      status_code == SC_CPIO_ENTITY_NOT_FOUND ||
      status_code == SC_CPIO_INVALID_ARGUMENT) {
    return KeyFetchingErrorType::kInvalidKeyId;
  }
  return KeyFetchingErrorType::kGenericError;
}

}  // namespace google::scp::cpio
