/*
 * Copyright 2022 Google LLC
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

namespace google::scp::core {
// TODO: Make the following configurable.
// The maximum thread count could be set.
static constexpr size_t kMaxThreadCount = 10000;
/// The maximum queue cap could be set.
static const size_t kMaxQueueCap = UINT_MAX;
/// The sleep interval for shutting down threads in miliseconds.
static const size_t kSleepDurationMs = 10;
/// Indicates an infinite wait time.
static constexpr std::chrono::nanoseconds kInfiniteWaitDurationNs =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::hours(87600));  // 10 years

/**
 * @brief Container for the statistics of a single threaded executor. This type
 * is shared by the normal and priority executor for simplicity.
 *
 */
struct SingleThreadExecutorStats {
  /**
   * @brief How many tasks were executed from the normal-priority queue.
   *
   */
  std::atomic_size_t num_normal_tasks_executed{0};
  /**
   * @brief How many tasks were executed from the high-priority queue
   *
   */
  std::atomic_size_t num_high_tasks_executed{0};
  /**
   * @brief How many tasks were executed from the urgent-priority queue.
   *
   */
  std::atomic_size_t num_urgent_tasks_executed{0};
};

struct StatsCollectionConfiguration {
  std::chrono::seconds metric_logging_interval{5};
};

}  // namespace google::scp::core
