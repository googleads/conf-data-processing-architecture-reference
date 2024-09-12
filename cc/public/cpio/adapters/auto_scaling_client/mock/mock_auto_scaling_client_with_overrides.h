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

#include <memory>

#include "cpio/client_providers/auto_scaling_client_provider/mock/mock_auto_scaling_client_provider.h"
#include "public/cpio/adapters/auto_scaling_client/src/auto_scaling_client.h"

namespace google::scp::cpio::mock {
class MockAutoScalingClientWithOverrides : public AutoScalingClient {
 public:
  MockAutoScalingClientWithOverrides(
      const std::shared_ptr<AutoScalingClientOptions>& options)
      : AutoScalingClient(options) {}

  core::ExecutionResult create_auto_scaling_client_provider_result =
      core::SuccessExecutionResult();

  core::ExecutionResult CreateAutoScalingClientProvider() noexcept override {
    if (create_auto_scaling_client_provider_result.Successful()) {
      auto_scaling_client_provider_ = std::make_shared<
          client_providers::mock::MockAutoScalingClientProvider>();
      return create_auto_scaling_client_provider_result;
    }
    return create_auto_scaling_client_provider_result;
  }

  std::shared_ptr<client_providers::mock::MockAutoScalingClientProvider>
  GetAutoScalingClientProvider() {
    return std::dynamic_pointer_cast<
        client_providers::mock::MockAutoScalingClientProvider>(
        auto_scaling_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock
