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

#include "ondemand_key_fetcher_with_cache.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "public/core/interface/execution_result_macros.h"
#include "public/cpio/utils/key_fetching/proto/key_coordinator_configuration.pb.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"

#include "key_fetching_utils.h"

using google::cmrt::sdk::v1::KeyCoordinatorConfiguration;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::KeyType;
using std::nullopt;
using std::optional;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using std::chrono::milliseconds;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace google::scp::cpio {
namespace {
constexpr char kOndemandKeyFetcherWithCacheComponentName[] =
    "EncryptionKeyFetcherWithCache";
constexpr milliseconds kLogPeriod = milliseconds(1000);

uint64_t GetKeyCacheLifetimeSeconds(
    const KeyFetcherOptions& key_fetcher_options) {
  return key_fetcher_options.key_cache_lifetime.count();
}

uint64_t GetFetchingFailureCacheLifetimeSeconds(
    const KeyFetcherOptions& key_fetcher_options) {
  return key_fetcher_options.fetching_failure_cache_lifetime.count();
}

}  // namespace

OndemandKeyFetcherWithCache::OndemandKeyFetcherWithCache(
    shared_ptr<AsyncExecutorInterface>& async_executor,
    PrivateKeyClientInterface& key_client,
    DualWritingMetricClientInterface& metric_client,
    const KeyCoordinatorConfiguration& key_service_options,
    KeyFetcherOptions key_fetcher_options, const std::string& metric_namespace)
    : CoordinatorKeyFetcherWithCacheBase(
          key_client, metric_client, key_service_options, key_fetcher_options,
          kOndemandKeyFetcherWithCacheComponentName, KeyType::kEncryptionKey,
          metric_namespace),
      key_cache_(
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
          async_executor, key_fetcher_options.use_read_lock_for_cache_read) {}

ExecutionResult OndemandKeyFetcherWithCache::Init() noexcept {
  RETURN_AND_LOG_IF_FAILURE(key_cache_.Init(),
                            kOndemandKeyFetcherWithCacheComponentName,
                            kZeroUuid, "Failed to init key_cache_.");
  RETURN_AND_LOG_IF_FAILURE(
      fetching_failure_cache_.Init(), kOndemandKeyFetcherWithCacheComponentName,
      kZeroUuid, "Failed to init fetching_failure_cache_.");
  return CoordinatorKeyFetcherWithCacheBase::Init();
}

ExecutionResult OndemandKeyFetcherWithCache::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(key_cache_.Run(),
                            kOndemandKeyFetcherWithCacheComponentName,
                            kZeroUuid, "Failed to run key_cache_.");
  RETURN_AND_LOG_IF_FAILURE(
      fetching_failure_cache_.Run(), kOndemandKeyFetcherWithCacheComponentName,
      kZeroUuid, "Failed to run fetching_failure_cache_.");
  return CoordinatorKeyFetcherWithCacheBase::Run();
}

ExecutionResult OndemandKeyFetcherWithCache::Stop() noexcept {
  RETURN_IF_FAILURE(CoordinatorKeyFetcherWithCacheBase::Stop());
  RETURN_AND_LOG_IF_FAILURE(key_cache_.Stop(),
                            kOndemandKeyFetcherWithCacheComponentName,
                            kZeroUuid, "Failed to stop key_cache_.");
  RETURN_AND_LOG_IF_FAILURE(
      fetching_failure_cache_.Stop(), kOndemandKeyFetcherWithCacheComponentName,
      kZeroUuid, "Failed to stop fetching_failure_cache_.");
  return SuccessExecutionResult();
}

void OndemandKeyFetcherWithCache::CacheValidKey(
    const vector<Key>& valid_keys) noexcept {
  for (auto valid_key : valid_keys) {
    Key key;
    pair<string, Key> key_pair;
    key_pair.first = valid_key.key_id;
    key_pair.second = std::move(valid_key);
    // Skip execution result checking and ignore the insert failure. Key cache
    // will refetch the key if the insertion failed.
    key_cache_.Insert(key_pair, key);
  }
}

void OndemandKeyFetcherWithCache::CacheFailureResult(
    std::string key_id, ExecutionResult failure_result) noexcept {
  pair<std::string, ExecutionResult> failed_key_pair;
  failed_key_pair.first = key_id;
  failed_key_pair.second = std::move(failure_result);
  // Remove the old failure result if it exits. Ignore the erase result.
  fetching_failure_cache_.Erase(key_id);
  // Ignore the insert result if another thread already insert it.
  fetching_failure_cache_.Insert(failed_key_pair, failure_result);
}

optional<Key> OndemandKeyFetcherWithCache::GetKeyFromValidKeyCache(
    const std::string& key_id) noexcept {
  Key key;
  auto execution_result = key_cache_.Find(key_id, key);
  if (execution_result.Successful()) {
    return key;
  }
  return nullopt;
}

optional<ExecutionResult>
OndemandKeyFetcherWithCache::GetFetchingFailureFromCache(
    const std::string& key_id) noexcept {
  ExecutionResult failure_result;
  auto is_key_found =
      fetching_failure_cache_.Find(key_id, failure_result).Successful();
  if (is_key_found) {
    SCP_ERROR_EVERY_PERIOD(
        kLogPeriod, kOndemandKeyFetcherWithCacheComponentName, kZeroUuid,
        failure_result, "The key fetching failure for %s is cached.",
        key_id.c_str());
    return failure_result;
  }
  return nullopt;
}

void OndemandKeyFetcherWithCache::MarkFetchingFinished(
    const string& key_id) noexcept {
  std::unique_lock lock(in_progress_key_cache_mutex_);
  in_progress_key_cache_.erase(key_id);
  lock.unlock();
}

bool OndemandKeyFetcherWithCache::MarkFetchingInProgress(
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

bool OndemandKeyFetcherWithCache::FetchingInProgress(
    const string& key_id) noexcept {
  bool in_progress = false;
  std::shared_lock lock(in_progress_key_cache_mutex_);
  auto it = in_progress_key_cache_.find(key_id);
  in_progress = it != in_progress_key_cache_.end();
  lock.unlock();
  return in_progress;
}
}  // namespace google::scp::cpio
