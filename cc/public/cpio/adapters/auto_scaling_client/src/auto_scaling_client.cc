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

#include "auto_scaling_client.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/common/adapter_utils.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::AutoScalingClientProviderFactory;
using google::scp::cpio::client_providers::AutoScalingClientProviderInterface;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

static constexpr char kAutoScalingClient[] = "AutoScalingClient";

namespace google::scp::cpio {
ExecutionResult AutoScalingClient::CreateAutoScalingClientProvider() noexcept {
  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(
          GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(cpu_async_executor)),
      kAutoScalingClient, kZeroUuid, "Failed to get CpuAsyncExecutor.");
  shared_ptr<AsyncExecutorInterface> io_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(
          GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor)),
      kAutoScalingClient, kZeroUuid, "Failed to get IoAsyncExecutor.");
  shared_ptr<InstanceClientProviderInterface> instance_client_provider;
  RETURN_AND_LOG_IF_FAILURE(
      ConvertToPublicExecutionResult(
          GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(
              instance_client_provider)),
      kAutoScalingClient, kZeroUuid, "Failed to get InstanceClientProvider.");
  auto_scaling_client_provider_ = AutoScalingClientProviderFactory::Create(
      options_, instance_client_provider, cpu_async_executor,
      io_async_executor);

  return SuccessExecutionResult();
}

ExecutionResult AutoScalingClient::Init() noexcept {
  auto execution_result = CreateAutoScalingClientProvider();
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kAutoScalingClient, kZeroUuid,
                            "Failed to create AutoScalingClientProvider.");
  execution_result = auto_scaling_client_provider_->Init();
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kAutoScalingClient, kZeroUuid,
                            "Failed to initialize AutoScalingClientProvider.");

  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult AutoScalingClient::Run() noexcept {
  auto execution_result = auto_scaling_client_provider_->Run();
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kAutoScalingClient, kZeroUuid,
                            "Failed to run AutoScalingClientProvider.");

  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult AutoScalingClient::Stop() noexcept {
  auto execution_result = auto_scaling_client_provider_->Stop();
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kAutoScalingClient, kZeroUuid,
                            "Failed to stop AutoScalingClientProvider.");

  return ConvertToPublicExecutionResult(execution_result);
}

void AutoScalingClient::TryFinishInstanceTermination(
    AsyncContext<TryFinishInstanceTerminationRequest,
                 TryFinishInstanceTerminationResponse>&
        try_finish_termination_context) noexcept {
  try_finish_termination_context.setConvertToPublicError(true);
  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_finish_termination_context);
}

ExecutionResultOr<TryFinishInstanceTerminationResponse>
AutoScalingClient::TryFinishInstanceTerminationSync(
    TryFinishInstanceTerminationRequest request) noexcept {
  TryFinishInstanceTerminationResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<TryFinishInstanceTerminationRequest,
                              TryFinishInstanceTerminationResponse>(
          bind(&AutoScalingClient::TryFinishInstanceTermination, this, _1),
          move(request), response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kAutoScalingClient, kZeroUuid,
                            "Failed to TryFinishInstanceTermination.");
  return response;
}

std::unique_ptr<AutoScalingClientInterface> AutoScalingClientFactory::Create(
    AutoScalingClientOptions options) {
  return make_unique<AutoScalingClient>(
      make_shared<AutoScalingClientOptions>(move(options)));
}
}  // namespace google::scp::cpio
