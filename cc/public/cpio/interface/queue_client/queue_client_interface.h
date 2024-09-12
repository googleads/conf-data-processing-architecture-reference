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

#ifndef SCP_CPIO_INTERFACE_QUEUE_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_QUEUE_CLIENT_INTERFACE_H_

#include <memory>
#include <string>

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {

/**
 * @brief Interface responsible for queuing messages.
 */
class QueueClientInterface : public core::ServiceInterface {
 public:
  virtual ~QueueClientInterface() = default;
  /**
   * @brief Enqueue a message to the queue.
   * @param enqueue_message_context context of the operation.
   */
  virtual void EnqueueMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context) noexcept = 0;
  /**
   * @brief Enqueue a message to the queue in a blocking call.
   * @param request request to enqueue message.
   * @return ExecutionResult<EnqueueMessageResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::queue_service::v1::EnqueueMessageResponse>
  EnqueueMessageSync(
      cmrt::sdk::queue_service::v1::EnqueueMessageRequest request) noexcept = 0;
  /**
   * @brief Get top message from the queue.
   * @param get_top_message_context context of the operation.
   */
  virtual void GetTopMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context) noexcept = 0;
  /**
   * @brief Get top message from the queue in a blocking call.
   * @param request request to get top message.
   * @return ExecutionResult<GetTopMessageResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::queue_service::v1::GetTopMessageResponse>
  GetTopMessageSync(
      cmrt::sdk::queue_service::v1::GetTopMessageRequest request) noexcept = 0;
  /**
   * @brief Update visibility timeout of a message from the queue.
   * @param update_message_visibility_timeout_context context of the operation.
   */
  virtual void UpdateMessageVisibilityTimeout(
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>&
          update_message_visibility_timeout_context) noexcept = 0;
  /**
   * @brief Update visibility timeout of a message from the queue in a blocking
   * call.
   * @param request request to update message visibility timeout.
   * @return ExecutionResult<UpdateMessageVisibilityTimeoutResponse> result of
   * the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>
  UpdateMessageVisibilityTimeoutSync(
      cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest
          request) noexcept = 0;
  /**
   * @brief Delete a message from the queue.
   * @param delete_message_context context of the operation.
   */
  virtual void DeleteMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_message_context) noexcept = 0;
  /**
   * @brief Delete a message from the queue in a blocking call.
   * @param request request to delete message.
   * @return ExecutionResultOr<DeleteMessageResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::queue_service::v1::DeleteMessageResponse>
  DeleteMessageSync(
      cmrt::sdk::queue_service::v1::DeleteMessageRequest request) noexcept = 0;
};

class QueueClientFactory {
 public:
  /**
   * @brief Factory to create QueueClientProvider.
   *
   * @param options QueueClientOptions.
   * @return std::unique_ptr<QueueClientInterface> created
   * QueueClientInterface.
   */
  static std::unique_ptr<QueueClientInterface> Create(
      QueueClientOptions options = QueueClientOptions()) noexcept;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_QUEUE_CLIENT_INTERFACE_H_
