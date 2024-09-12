/*
 * Copyright 2023 Google LLC
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

#include <string>

#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/metric_instance/interface/aggregate_metric_interface.h"

namespace google::scp::cpio {
class NoopAggregateMetric : public AggregateMetricInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Increment(
      const std::string& event_code) noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult IncrementBy(
      uint64_t value, const std::string& event_code) noexcept override {
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::cpio
