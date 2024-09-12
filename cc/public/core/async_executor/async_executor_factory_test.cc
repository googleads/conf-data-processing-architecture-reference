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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/async_executor/src/async_executor.h"
#include "public/core/async_executor/async_executor_factory.h"

using ::testing::NotNull;
using ::testing::Pointer;
using ::testing::WhenDynamicCastTo;

namespace google::scp::core::test {

TEST(AsyncExecutorPublicTests, CreateAsyncExecutorSuccessfully) {
  AsyncExecutorFactory factory;
  auto async_executor =
      factory.CreateAsyncExecutor({.thread_count = 10,
                                   .queue_cap = 10,
                                   .drop_tasks_on_stop = true,
                                   .enable_stats_keeping = true});
  EXPECT_THAT(async_executor,
              Pointer(WhenDynamicCastTo<const AsyncExecutor*>(NotNull())));
}
}  // namespace google::scp::core::test
