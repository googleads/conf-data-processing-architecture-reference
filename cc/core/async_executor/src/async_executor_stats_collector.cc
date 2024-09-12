// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "async_executor_stats_collector.h"

#include <string_view>
#include <memory>
#include <string>
#include <utility>
#include <map>
#include <thread>

#include "core/common/uuid/src/uuid.h"
#include "core/common/global_logger/src/global_logger.h"

using google::scp::core::common::kZeroUuid;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string_view;
using std::thread;
using std::string;
using std::this_thread::sleep_for;

namespace google::scp::core {
ExecutionResult AsyncExecutorStatsCollector::Init() noexcept {
  return SuccessExecutionResult();
};

ExecutionResult AsyncExecutorStatsCollector::Run() noexcept {
  is_running_ = true;
  collector_thread_ = thread([this]() {
    while (is_running_.load()) {
      sleep_for(config_.metric_logging_interval);
      for (const auto& [name, executor] : executors_) {
        auto stats = executor->GetStatistics();
        float normal_task_latency_millis =
            stats.num_normal_tasks_executed != 0
                ? (config_.metric_logging_interval.count() * 1000.0f) /
                      stats.num_normal_tasks_executed
                : 0;
        float high_task_latency_millis =
            stats.num_high_tasks_executed != 0
                ? (config_.metric_logging_interval.count() * 1000.0f) /
                      stats.num_high_tasks_executed
                : 0;
        float urgent_task_latency_millis =
            stats.num_urgent_tasks_executed != 0
                ? (config_.metric_logging_interval.count() * 1000.0f) /
                      stats.num_urgent_tasks_executed
                : 0;

        SCP_DEBUG(name.c_str(), kZeroUuid,
                  "%lu normal tasks executed: %f millis average",
                  stats.num_normal_tasks_executed, normal_task_latency_millis);
        SCP_DEBUG(name.c_str(), kZeroUuid,
                  "%lu high tasks executed: %f millis average",
                  stats.num_high_tasks_executed, high_task_latency_millis);
        SCP_DEBUG(name.c_str(), kZeroUuid,
                  "%lu urgent tasks executed: %f millis average",
                  stats.num_urgent_tasks_executed, urgent_task_latency_millis);
        SCP_DEBUG(name.c_str(), kZeroUuid,
                  "Queue sizes: [%lu, %lu, %lu] normal, high, urgent",
                  stats.normal_task_queue_size, stats.high_task_queue_size,
                  stats.urgent_task_queue_size);
      }
    }
  });
  return SuccessExecutionResult();
}

ExecutionResult AsyncExecutorStatsCollector::Stop() noexcept {
  is_running_ = false;
  if (collector_thread_.joinable()) {
    collector_thread_.join();
  }
  return SuccessExecutionResult();
}

void AsyncExecutorStatsCollector::AddExecutor(
    string_view name, shared_ptr<AsyncExecutorInterface> executor) {
  executors_[string(name)] = executor;
}

}  // namespace google::scp::core
