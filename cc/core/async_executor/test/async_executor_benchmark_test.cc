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

#include <gtest/gtest.h>

#include <atomic>
#include <mutex>

#include <benchmark/benchmark.h>

#include "core/async_executor/src/async_executor.h"
#include "public/core/test/interface/execution_result_matchers.h"

using std::make_shared;
using std::shared_ptr;

namespace google::scp::core::test {

static constexpr size_t kDepth = 10;

static std::mutex global_hashes_mutex;
static std::vector<size_t> global_hashes;

static void WorkFunction(shared_ptr<AsyncExecutorInterface> async_executor,
                         std::atomic<size_t>& task_completion_counter,
                         size_t task_size, size_t max_depth,
                         size_t current_depth) {
  // Work (hash) for some iterations
  for (int i = 0; i < task_size; i++) {
    auto time_ticks =
        std::chrono::steady_clock::now().time_since_epoch().count();
    auto hash = std::hash<std::string>()("string" + std::to_string(time_ticks));
    {
      std::unique_lock lock(global_hashes_mutex);
      global_hashes.push_back(hash);
    }
  }
  task_completion_counter++;
  if (current_depth == max_depth) {
    return;
  }

  // Schedule another one.
  current_depth++;
  EXPECT_SUCCESS(
      async_executor->Schedule(std::bind(&WorkFunction, async_executor,
                                         std::ref(task_completion_counter),
                                         task_size, max_depth, current_depth),
                               AsyncPriority::Normal));
}

static size_t BenchmarkWorkFunction(
    shared_ptr<AsyncExecutorInterface> async_executor, size_t task_size,
    size_t num_tasks_per_depth, size_t max_depth) {
  EXPECT_SUCCESS(async_executor->Init());
  EXPECT_SUCCESS(async_executor->Run());
  std::atomic<size_t> task_completion_counter = 0;
  for (int i = 0; i < num_tasks_per_depth; i++) {
    EXPECT_SUCCESS(async_executor->Schedule(
        std::bind(WorkFunction, async_executor,
                  std::ref(task_completion_counter), task_size, max_depth,
                  /*current_depth=*/1),
        AsyncPriority::Normal));
  }
  while (task_completion_counter < (num_tasks_per_depth * max_depth)) {}
  EXPECT_SUCCESS(async_executor->Stop());
  return task_completion_counter;
}

static void BM_TaskAssignmentGlobalRoundRobin(benchmark::State& state) {
  for (auto _ : state) {
    auto executor_round_robin_global = make_shared<AsyncExecutor>(
        std::thread::hardware_concurrency(), 100000, /*drop_tasks=*/false,
        AsyncExecutorTaskLoadBalancingScheme::RoundRobinGlobal);
    state.counters["TaskCount"] = BenchmarkWorkFunction(
        executor_round_robin_global, state.range(0), state.range(1), kDepth);
  }
}

static void BM_TaskAssignmentThreadRoundRobin(benchmark::State& state) {
  for (auto _ : state) {
    auto executor_round_robin_per_thread = make_shared<AsyncExecutor>(
        std::thread::hardware_concurrency(), 100000, /*drop_tasks=*/false,
        AsyncExecutorTaskLoadBalancingScheme::RoundRobinPerThread);
    state.counters["TaskCount"] =
        BenchmarkWorkFunction(executor_round_robin_per_thread, state.range(0),
                              state.range(1), kDepth);
  }
}

static void BM_TaskAssignmentGlobalRandom(benchmark::State& state) {
  for (auto _ : state) {
    auto executor_random = make_shared<AsyncExecutor>(
        std::thread::hardware_concurrency(), 100000,
        /*drop_tasks=*/false, AsyncExecutorTaskLoadBalancingScheme::Random);
    state.counters["TaskCount"] = BenchmarkWorkFunction(
        executor_random, state.range(0), state.range(1), kDepth);
  }
}
}  // namespace google::scp::core::test

// ArgPair<Task Size, Number of Tasks>
BENCHMARK(google::scp::core::test::BM_TaskAssignmentGlobalRoundRobin)
    ->ArgPair(1, 10000)
    ->ArgPair(10, 10000)
    ->ArgPair(100, 10000)
    ->ArgPair(1000, 10000);

// ArgPair<Task Size, Number of Tasks>
BENCHMARK(google::scp::core::test::BM_TaskAssignmentThreadRoundRobin)
    ->ArgPair(1, 10000)
    ->ArgPair(10, 10000)
    ->ArgPair(100, 10000)
    ->ArgPair(1000, 10000);

// ArgPair<Task Size, Number of Tasks>
BENCHMARK(google::scp::core::test::BM_TaskAssignmentGlobalRandom)
    ->ArgPair(1, 10000)
    ->ArgPair(10, 10000)
    ->ArgPair(100, 10000)
    ->ArgPair(1000, 10000);

// Run the benchmark
BENCHMARK_MAIN();
