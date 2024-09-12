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

#include "async_executor_factory.h"

#include <functional>
#include <memory>

#include "core/async_executor/src/async_executor.h"
#include "core/interface/async_executor_interface.h"

using std::make_unique;
using std::unique_ptr;

namespace google::scp::core {
unique_ptr<AsyncExecutorInterface> AsyncExecutorFactory::CreateAsyncExecutor(
    const AsyncExecutorOptions& options) {
  return make_unique<AsyncExecutor>(
      options.thread_count, options.queue_cap, options.drop_tasks_on_stop,
      options.task_load_balancing_scheme, options.enable_stats_keeping);
}
}  // namespace google::scp::core
