// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpio/client_providers/job_client_provider/src/gcp/gcp_job_client_provider.h"

#include "cc/cpio/client_providers/job_client_provider/src/error_codes.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/test/utils/scp_test_base.h"
#include "cpio/client_providers/nosql_database_client_provider/mock/mock_nosql_database_client_provider.h"
#include "cpio/client_providers/queue_client_provider/mock/mock_queue_client_provider.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_ALREADY_EXISTS;
using google::scp::core::errors::SC_GCP_NOT_FOUND;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_JOB_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_JOB_ENTRY_CREATION_FAILED;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::cpio::client_providers::mock::
    MockNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::mock::MockQueueClientProvider;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;
using testing::NiceMock;

namespace {
constexpr char kJobsQueueName[] = "Queue";
constexpr char kJobsTableName[] = "Jobs";
constexpr char kJobsSpannerInstanceName[] = "Instance";
constexpr char kJobsSpannerDatabaseName[] = "Database";
}  // namespace

namespace google::scp::cpio::client_providers::job_client::test {

class GcpJobClientProviderTest : public ScpTestBase {
 protected:
  GcpJobClientProviderTest() {
    job_client_options_ = make_shared<JobClientOptions>();
    job_client_options_->job_queue_name = kJobsQueueName;
    job_client_options_->job_table_name = kJobsTableName;
    job_client_options_->gcp_spanner_instance_name = kJobsSpannerInstanceName;
    job_client_options_->gcp_spanner_database_name = kJobsSpannerDatabaseName;
    queue_client_provider_ = make_shared<NiceMock<MockQueueClientProvider>>();
    nosql_database_client_provider_ =
        make_shared<NiceMock<MockNoSQLDatabaseClientProvider>>();

    gcp_job_client_provider_ = make_unique<GcpJobClientProvider>(
        job_client_options_, queue_client_provider_,
        nosql_database_client_provider_, make_shared<MockAsyncExecutor>());

    EXPECT_SUCCESS(gcp_job_client_provider_->Init());
    EXPECT_SUCCESS(gcp_job_client_provider_->Run());
  }

  void TearDown() override { EXPECT_SUCCESS(gcp_job_client_provider_->Stop()); }

  shared_ptr<JobClientOptions> job_client_options_;
  shared_ptr<MockQueueClientProvider> queue_client_provider_;
  shared_ptr<MockNoSQLDatabaseClientProvider> nosql_database_client_provider_;
  unique_ptr<GcpJobClientProvider> gcp_job_client_provider_;
};

TEST_F(GcpJobClientProviderTest, InitWithEmptySpannerInstanceName) {
  auto job_client_options = make_shared<JobClientOptions>();
  job_client_options->job_queue_name = kJobsQueueName;
  job_client_options->job_table_name = kJobsTableName;
  job_client_options->gcp_spanner_database_name = kJobsSpannerDatabaseName;
  auto client = make_unique<GcpJobClientProvider>(
      job_client_options, queue_client_provider_,
      nosql_database_client_provider_, make_shared<MockAsyncExecutor>());

  EXPECT_THAT(client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_JOB_CLIENT_PROVIDER_JOB_CLIENT_OPTIONS_REQUIRED)));
}

TEST_F(GcpJobClientProviderTest, InitWithEmptySpannerDatabaseName) {
  auto job_client_options = make_shared<JobClientOptions>();
  job_client_options->job_queue_name = kJobsQueueName;
  job_client_options->job_table_name = kJobsTableName;
  job_client_options->gcp_spanner_instance_name = kJobsSpannerInstanceName;
  auto client = make_unique<GcpJobClientProvider>(
      job_client_options, queue_client_provider_,
      nosql_database_client_provider_, make_shared<MockAsyncExecutor>());

  EXPECT_THAT(client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_JOB_CLIENT_PROVIDER_JOB_CLIENT_OPTIONS_REQUIRED)));
}

TEST_F(GcpJobClientProviderTest,
       ConvertDatabaseErrorForPutJobWithConditionFailure) {
  auto status_code = SC_GCP_ALREADY_EXISTS;
  EXPECT_THAT(
      gcp_job_client_provider_->ConvertDatabaseErrorForPutJob(status_code),
      ResultIs(FailureExecutionResult(
          SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION)));
}

TEST_F(GcpJobClientProviderTest,
       ConvertDatabaseErrorForPutJobWithOtherFailure) {
  auto status_code = SC_GCP_NOT_FOUND;
  EXPECT_THAT(
      gcp_job_client_provider_->ConvertDatabaseErrorForPutJob(status_code),
      ResultIs(FailureExecutionResult(
          SC_JOB_CLIENT_PROVIDER_JOB_ENTRY_CREATION_FAILED)));
}
}  // namespace google::scp::cpio::client_providers::job_client::test
