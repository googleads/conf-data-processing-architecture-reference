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

#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/job_client/src/job_client.h"
#include "public/cpio/test/job_client/test_gcp_job_client_options.h"

namespace google::scp::cpio {
/*! @copydoc JobClient
 */
class TestGcpJobClient : public JobClient {
 public:
  explicit TestGcpJobClient(
      const std::shared_ptr<TestGcpJobClientOptions>& options)
      : JobClient(options) {}

  core::ExecutionResultOr<std::shared_ptr<QueueClientOptions>>
  CreateQueueClientOptions() noexcept override;
  core::ExecutionResultOr<std::shared_ptr<NoSQLDatabaseClientOptions>>
  CreateNoSQLDatabaseClientOptions() noexcept override;
};
}  // namespace google::scp::cpio
