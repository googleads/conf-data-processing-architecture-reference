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

#ifndef SCP_CPIO_INTERFACE_AUTO_SCALING_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_AUTO_SCALING_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for providing functionalities to avoid Cloud
 * AutoScaling to interrupt task's execution.
 *
 * Use AwsAutoScalingClientFactory::Create to create the AutoScalingClient. Call
 * AutoScalingClientInterface::Init and AutoScalingClientInterface::Run before
 * actually use it, and call AutoScalingClientInterface::Stop when finish using
 * it.
 */
class AutoScalingClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief If the given instance is scheduled to be terminated, terminate the
   * instance immediately. If not, do nothing.
   *
   * @param try_finish_termination_context the context of the operation.
   * @return core::ExecutionResult the execution result of the operation.
   */
  virtual void TryFinishInstanceTermination(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>&
          try_finish_termination_context) noexcept = 0;

  /**
   * @brief If the given instance is scheduled to be terminated, terminate the
   * instance immediately. If not, do nothing.
   *
   * @param request request to try to terminate instance.
   * @return core::ExecutionResultOr<TryFinishInstanceTerminationResponse> the
   * execution result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::auto_scaling_service::v1::TryFinishInstanceTerminationResponse>
  TryFinishInstanceTerminationSync(
      cmrt::sdk::auto_scaling_service::v1::TryFinishInstanceTerminationRequest
          request) noexcept = 0;
};

/// Factory to create AutoScalingClient.
class AutoScalingClientFactory {
 public:
  /**
   * @brief Creates AutoScalingClient.
   *
   * @param options configurations for AutoScalingClient.
   * @return std::unique_ptr<AutoScalingClientInterface> AutoScalingClient
   * object.
   */
  static std::unique_ptr<AutoScalingClientInterface> Create(
      AutoScalingClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_AUTO_SCALING_CLIENT_INTERFACE_H_
