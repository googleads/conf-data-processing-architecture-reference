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

#include <gmock/gmock.h>

#include <memory>
#include <string>

#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/metric_instance/interface/simple_metric_interface.h"
#include "public/cpio/utils/metric_instance/interface/type_def.h"

namespace google::scp::cpio {
class MockSimpleMetric : public SimpleMetricInterface {
 public:
  MockSimpleMetric() {
    ON_CALL(*this, Init)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Push).WillByDefault(testing::Return());
  }

  MOCK_METHOD(core::ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Run, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(void, Push,
              (const std::string&,
               std::optional<std::reference_wrapper<const MetricDefinition>>),
              (noexcept, override));
};
}  // namespace google::scp::cpio