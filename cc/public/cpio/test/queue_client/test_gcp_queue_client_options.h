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

#ifndef SCP_CPIO_TEST_GCP_QUEUE_CLIENT_OPTIONS_H_
#define SCP_CPIO_TEST_GCP_QUEUE_CLIENT_OPTIONS_H_

#include <memory>
#include <string>

#include "public/cpio/interface/queue_client/type_def.h"

namespace google::scp::cpio {
/// QueueClientOptions for testing on GCP.
struct TestGcpQueueClientOptions : public QueueClientOptions {
  virtual ~TestGcpQueueClientOptions() = default;

  TestGcpQueueClientOptions() = default;

  explicit TestGcpQueueClientOptions(const QueueClientOptions& options)
      : QueueClientOptions(options) {}

  std::string pubsub_endpoint_override;

  std::string access_token;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_TEST_GCP_QUEUE_CLIENT_OPTIONS_H_
