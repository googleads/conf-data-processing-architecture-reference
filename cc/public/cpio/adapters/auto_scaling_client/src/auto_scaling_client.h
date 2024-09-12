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
#include <string>

#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/auto_scaling_client/auto_scaling_client_interface.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc AutoScalingClientInterface
 */
class AutoScalingClient : public AutoScalingClientInterface {
 public:
  explicit AutoScalingClient(
      const std::shared_ptr<AutoScalingClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  void TryFinishInstanceTermination(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>&
          try_finish_termination_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::auto_scaling_service::v1::TryFinishInstanceTerminationResponse>
  TryFinishInstanceTerminationSync(
      cmrt::sdk::auto_scaling_service::v1::TryFinishInstanceTerminationRequest
          request) noexcept override;

 protected:
  virtual core::ExecutionResult CreateAutoScalingClientProvider() noexcept;

  std::shared_ptr<client_providers::AutoScalingClientProviderInterface>
      auto_scaling_client_provider_;

 private:
  std::shared_ptr<AutoScalingClientOptions> options_;
};
}  // namespace google::scp::cpio
