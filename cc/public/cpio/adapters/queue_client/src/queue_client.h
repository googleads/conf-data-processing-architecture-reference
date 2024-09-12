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

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/queue_client/queue_client_interface.h"
#include "public/cpio/interface/queue_client/type_def.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

namespace google::scp::cpio {

/**
 * @brief Interface responsible for queuing messages.
 */
class QueueClient : public QueueClientInterface {
 public:
  explicit QueueClient(const std::shared_ptr<QueueClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  void EnqueueMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context) noexcept override;

  core::ExecutionResultOr<cmrt::sdk::queue_service::v1::EnqueueMessageResponse>
  EnqueueMessageSync(cmrt::sdk::queue_service::v1::EnqueueMessageRequest
                         request) noexcept override;

  void GetTopMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context) noexcept override;

  core::ExecutionResultOr<cmrt::sdk::queue_service::v1::GetTopMessageResponse>
  GetTopMessageSync(cmrt::sdk::queue_service::v1::GetTopMessageRequest
                        request) noexcept override;

  void UpdateMessageVisibilityTimeout(
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>&
          update_message_visibility_timeout_context) noexcept override;

  core::ExecutionResultOr<
      cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>
  UpdateMessageVisibilityTimeoutSync(
      cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest
          request) noexcept override;

  void DeleteMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_message_context) noexcept override;

  core::ExecutionResultOr<cmrt::sdk::queue_service::v1::DeleteMessageResponse>
  DeleteMessageSync(cmrt::sdk::queue_service::v1::DeleteMessageRequest
                        request) noexcept override;

 protected:
  std::shared_ptr<client_providers::QueueClientProviderInterface>
      queue_client_provider_;
  std::shared_ptr<QueueClientOptions> options_;
};
}  // namespace google::scp::cpio
