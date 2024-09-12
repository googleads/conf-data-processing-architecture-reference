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

#ifndef SCP_CPIO_INTERFACE_QUEUE_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_QUEUE_CLIENT_TYPE_DEF_H_

#include <string>

namespace google::scp::cpio {
/// Configurations for QueueClient.
struct QueueClientOptions {
  virtual ~QueueClientOptions() = default;

  QueueClientOptions() = default;

  QueueClientOptions(const QueueClientOptions& options)
      : queue_name(options.queue_name) {}

  /**
   * @brief Required. The identifier of the queue. The queue is per client per
   * service. In AWS SQS, it's the queue name. In GCP Pub/Sub, there is only one
   * Subscription subscribes to the Topic, so the queue name is tied to Topic Id
   * and Subscription Id.
   *
   */
  std::string queue_name;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_QUEUE_CLIENT_TYPE_DEF_H_
