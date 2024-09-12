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

#include "test_gcp_job_client.h"

#include <functional>
#include <memory>
#include <utility>

#include "cc/cpio/common/src/common_error_codes.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "cpio/client_providers/queue_client_provider/test/gcp/test_gcp_queue_client_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"
#include "public/cpio/test/nosql_database_client/test_gcp_nosql_database_client_options.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_COMMON_ERRORS_POINTER_CASTING_FAILURE;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using std::make_shared;
using std::shared_ptr;

namespace {
constexpr char kTestGcpJobClient[] = "TestGcpJobClient";
}  // namespace

namespace google::scp::cpio {
ExecutionResultOr<shared_ptr<QueueClientOptions>>
TestGcpJobClient::CreateQueueClientOptions() noexcept {
  auto queue_options = make_shared<TestGcpQueueClientOptions>();
  queue_options->queue_name = options_->job_queue_name;
  auto test_job_client_options =
      dynamic_pointer_cast<TestGcpJobClientOptions>(options_);
  if (!test_job_client_options) {
    auto failure =
        FailureExecutionResult(SC_COMMON_ERRORS_POINTER_CASTING_FAILURE);
    SCP_ERROR(kTestGcpJobClient, kZeroUuid, failure,
              "Failed to cast JobClientOptions to TestGcpJobClientOptions.");
    return failure;
  }
  queue_options->access_token = test_job_client_options->access_token;
  queue_options->pubsub_endpoint_override =
      test_job_client_options->pubsub_endpoint_override;
  return queue_options;
}

ExecutionResultOr<shared_ptr<NoSQLDatabaseClientOptions>>
TestGcpJobClient::CreateNoSQLDatabaseClientOptions() noexcept {
  auto nosql_database_options =
      make_shared<TestGcpNoSQLDatabaseClientOptions>();
  nosql_database_options->gcp_spanner_instance_name =
      options_->gcp_spanner_instance_name;
  nosql_database_options->gcp_spanner_database_name =
      options_->gcp_spanner_database_name;
  auto test_job_client_options =
      dynamic_pointer_cast<TestGcpJobClientOptions>(options_);
  if (!test_job_client_options) {
    auto failure =
        FailureExecutionResult(SC_COMMON_ERRORS_POINTER_CASTING_FAILURE);
    SCP_ERROR(kTestGcpJobClient, kZeroUuid, failure,
              "Failed to cast JobClientOptions to TestGcpJobClientOptions.");
    return failure;
  }
  nosql_database_options->impersonate_service_account =
      test_job_client_options->impersonate_service_account;
  nosql_database_options->spanner_endpoint_override =
      test_job_client_options->spanner_endpoint_override;
  return nosql_database_options;
}
}  // namespace google::scp::cpio
