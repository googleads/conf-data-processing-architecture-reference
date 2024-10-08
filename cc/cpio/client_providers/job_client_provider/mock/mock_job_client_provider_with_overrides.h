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
#include "cpio/client_providers/job_client_provider/src/job_client_provider.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockJobClientProviderWithOverrides : public JobClientProvider {
 public:
  MockJobClientProviderWithOverrides(
      const std::shared_ptr<JobClientOptions>& job_client_options,
      const std::shared_ptr<QueueClientProviderInterface>&
          queue_client_provider,
      const std::shared_ptr<NoSQLDatabaseClientProviderInterface>&
          nosql_database_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor)
      : JobClientProvider(job_client_options, queue_client_provider,
                          nosql_database_client_provider, io_async_executor) {}

  core::ExecutionResult ConvertDatabaseErrorForPutJob(
      const core::StatusCode status_code_from_database) noexcept {
    return google::scp::core::FailureExecutionResult(status_code_from_database);
  };
};
}  // namespace google::scp::cpio::client_providers::mock
