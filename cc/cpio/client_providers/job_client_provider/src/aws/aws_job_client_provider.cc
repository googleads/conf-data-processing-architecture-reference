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
#include "aws_job_client_provider.h"

#include <memory>

#include "cpio/client_providers/job_client_provider/src/error_codes.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_JOB_ENTRY_CREATION_FAILED;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_CONDITIONAL_CHECKED_FAILED;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio::client_providers {
ExecutionResult AwsJobClientProvider::ConvertDatabaseErrorForPutJob(
    const StatusCode status_code_from_database) noexcept {
  if (status_code_from_database ==
      SC_NO_SQL_DATABASE_PROVIDER_CONDITIONAL_CHECKED_FAILED) {
    return FailureExecutionResult(
        SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION);
  } else {
    return FailureExecutionResult(
        SC_JOB_CLIENT_PROVIDER_JOB_ENTRY_CREATION_FAILED);
  }
}

#ifndef TEST_CPIO
shared_ptr<JobClientProviderInterface> JobClientProviderFactory::Create(
    const shared_ptr<JobClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface> instance_client,
    const shared_ptr<QueueClientProviderInterface> queue_client,
    const shared_ptr<NoSQLDatabaseClientProviderInterface>
        nosql_database_client,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>&
        io_async_executor) noexcept {
  return make_shared<AwsJobClientProvider>(
      options, queue_client, nosql_database_client, io_async_executor);
}
#endif
}  // namespace google::scp::cpio::client_providers
