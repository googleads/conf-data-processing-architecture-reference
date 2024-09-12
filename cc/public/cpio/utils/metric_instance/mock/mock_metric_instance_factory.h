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

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "public/cpio/utils/metric_instance/interface/metric_instance_factory_interface.h"

#include "mock_aggregate_metric.h"
#include "mock_simple_metric.h"

namespace google::scp::cpio {
class MockMetricInstanceFactory : public MetricInstanceFactoryInterface {
 public:
  MockMetricInstanceFactory() {
    ON_CALL(*this, ConstructSimpleMetricInstance).WillByDefault([] {
      return std::make_unique<MockSimpleMetric>();
    });
    ON_CALL(*this, ConstructAggregateMetricInstance(testing::_))
        .WillByDefault([] { return std::make_unique<MockAggregateMetric>(); });
    ON_CALL(*this, ConstructAggregateMetricInstance(testing::_, testing::_))
        .WillByDefault([] { return std::make_unique<MockAggregateMetric>(); });
    ON_CALL(*this, ConstructAggregateMetricInstance(testing::_, testing::_,
                                                    testing::_))
        .WillByDefault([] { return std::make_unique<MockAggregateMetric>(); });
  }

  MOCK_METHOD(std::unique_ptr<SimpleMetricInterface>,
              ConstructSimpleMetricInstance, ((MetricDefinition)),
              (noexcept, override));

  MOCK_METHOD(std::unique_ptr<AggregateMetricInterface>,
              ConstructAggregateMetricInstance, ((MetricDefinition)),
              (noexcept, override));

  MOCK_METHOD(std::unique_ptr<AggregateMetricInterface>,
              ConstructAggregateMetricInstance,
              (MetricDefinition, const std::vector<std::string>&),
              (noexcept, override));

  MOCK_METHOD(std::unique_ptr<AggregateMetricInterface>,
              ConstructAggregateMetricInstance,
              (MetricDefinition, const std::vector<std::string>&,
               const std::string&),
              (noexcept, override));
};
}  // namespace google::scp::cpio
