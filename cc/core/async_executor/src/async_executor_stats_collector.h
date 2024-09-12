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

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "core/interface/async_executor_interface.h"
#include "core/interface/service_interface.h"

#include "typedef.h"

namespace google::scp::core {

class AsyncExecutorStatsCollector : public ServiceInterface {
 public:
  explicit AsyncExecutorStatsCollector(StatsCollectionConfiguration config)
      : config_(std::move(config)) {}

  ExecutionResult Init() noexcept;
  ExecutionResult Run() noexcept;
  ExecutionResult Stop() noexcept;

  void AddExecutor(std::string_view name,
                   std::shared_ptr<AsyncExecutorInterface> executor);

 private:
  StatsCollectionConfiguration config_;
  std::map<std::string, std::shared_ptr<AsyncExecutorInterface>> executors_;

  std::atomic_bool is_running_{false};
  std::thread collector_thread_;
};

}  // namespace google::scp::core
