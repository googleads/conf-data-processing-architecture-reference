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

#include "queue_client.h"

#include <functional>
#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"
#include "public/cpio/test/queue_client/test_gcp_queue_client_options.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

using google::cmrt::sdk::queue_service::v1::DeleteMessageRequest;
using google::cmrt::sdk::queue_service::v1::DeleteMessageResponse;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageRequest;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageResponse;
using google::cmrt::sdk::queue_service::v1::GetTopMessageRequest;
using google::cmrt::sdk::queue_service::v1::GetTopMessageResponse;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutRequest;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::QueueClientProviderFactory;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using std::placeholders::_1;

namespace {
constexpr char kQueueClient[] = "QueueClient";
}  // namespace

namespace google::scp::cpio {
ExecutionResult QueueClient::Init() noexcept {
  shared_ptr<InstanceClientProviderInterface> instance_client;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(instance_client),
      kQueueClient, kZeroUuid, "Failed to get InstanceClientProvider.");

  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(cpu_async_executor),
      kQueueClient, kZeroUuid, "Failed to get CpuAsyncExecutor.");

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor),
      kQueueClient, kZeroUuid, "Failed to get IoAsyncExecutor.");

  queue_client_provider_ = QueueClientProviderFactory::Create(
      options_, instance_client, cpu_async_executor, io_async_executor);
  RETURN_AND_LOG_IF_FAILURE(queue_client_provider_->Init(), kQueueClient,
                            kZeroUuid,
                            "Failed to initialize QueueClientProvider.");

  return SuccessExecutionResult();
}

ExecutionResult QueueClient::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(queue_client_provider_->Run(), kQueueClient,
                            kZeroUuid, "Failed to run QueueClientProvider.");
  return SuccessExecutionResult();
}

ExecutionResult QueueClient::Stop() noexcept {
  RETURN_AND_LOG_IF_FAILURE(queue_client_provider_->Stop(), kQueueClient,
                            kZeroUuid, "Failed to run QueueClientProvider.");
  return SuccessExecutionResult();
}

void QueueClient::EnqueueMessage(
    AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
        enqueue_message_context) noexcept {
  queue_client_provider_->EnqueueMessage(enqueue_message_context);
}

ExecutionResultOr<EnqueueMessageResponse> QueueClient::EnqueueMessageSync(
    EnqueueMessageRequest request) noexcept {
  EnqueueMessageResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<EnqueueMessageRequest, EnqueueMessageResponse>(
          bind(&QueueClient::EnqueueMessage, this, _1), move(request),
          response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kQueueClient, kZeroUuid,
                            "Failed to enqueue message.");
  return response;
}

void QueueClient::GetTopMessage(
    AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
        get_top_message_context) noexcept {
  queue_client_provider_->GetTopMessage(get_top_message_context);
}

ExecutionResultOr<GetTopMessageResponse> QueueClient::GetTopMessageSync(
    GetTopMessageRequest request) noexcept {
  GetTopMessageResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetTopMessageRequest, GetTopMessageResponse>(
          bind(&QueueClient::GetTopMessage, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kQueueClient, kZeroUuid,
                            "Failed to get top message.");
  return response;
}

void QueueClient::UpdateMessageVisibilityTimeout(
    AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                 UpdateMessageVisibilityTimeoutResponse>&
        update_message_visibility_timeout_context) noexcept {
  queue_client_provider_->UpdateMessageVisibilityTimeout(
      update_message_visibility_timeout_context);
}

ExecutionResultOr<UpdateMessageVisibilityTimeoutResponse>
QueueClient::UpdateMessageVisibilityTimeoutSync(
    UpdateMessageVisibilityTimeoutRequest request) noexcept {
  UpdateMessageVisibilityTimeoutResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<UpdateMessageVisibilityTimeoutRequest,
                              UpdateMessageVisibilityTimeoutResponse>(
          bind(&QueueClient::UpdateMessageVisibilityTimeout, this, _1),
          move(request), response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kQueueClient, kZeroUuid,
                            "Failed to update message visibility timeout.");
  return response;
}

void QueueClient::DeleteMessage(
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_message_context) noexcept {
  queue_client_provider_->DeleteMessage(delete_message_context);
}

ExecutionResultOr<DeleteMessageResponse> QueueClient::DeleteMessageSync(
    DeleteMessageRequest request) noexcept {
  DeleteMessageResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<DeleteMessageRequest, DeleteMessageResponse>(
          bind(&QueueClient::DeleteMessage, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kQueueClient, kZeroUuid,
                            "Failed to delete message.");
  return response;
}

unique_ptr<QueueClientInterface> QueueClientFactory::Create(
    QueueClientOptions options) noexcept {
  return make_unique<QueueClient>(make_shared<QueueClientOptions>(options));
}
}  // namespace google::scp::cpio
