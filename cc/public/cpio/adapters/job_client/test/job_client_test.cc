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

#include "public/cpio/adapters/job_client/src/job_client.h"

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/job_client/mock/mock_job_client_with_overrides.h"
#include "public/cpio/interface/job_client/job_client_interface.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"

using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest;
using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse;
using google::cmrt::sdk::job_service::v1::GetJobByIdRequest;
using google::cmrt::sdk::job_service::v1::GetJobByIdResponse;
using google::cmrt::sdk::job_service::v1::GetNextJobRequest;
using google::cmrt::sdk::job_service::v1::GetNextJobResponse;
using google::cmrt::sdk::job_service::v1::PutJobRequest;
using google::cmrt::sdk::job_service::v1::PutJobResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse;
using google::scp::core::AsyncContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CPIO_UNKNOWN_ERROR;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::mock::MockJobClientWithOverrides;
using std::atomic;
using std::atomic_bool;
using std::make_shared;
using testing::Return;

namespace google::scp::cpio::test {

class JobClientTest : public ScpTestBase {
 protected:
  JobClientTest() { assert(client_.Init().Successful()); }

  MockJobClientWithOverrides client_;
};

TEST_F(JobClientTest, PutJobSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), PutJob)
      .WillOnce([=](auto& context) {
        context.response = make_shared<PutJobResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<PutJobRequest, PutJobResponse>(
      make_shared<PutJobRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(PutJobResponse()));
        finished = true;
      });
  client_.PutJob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, PutJobFailure) {
  EXPECT_CALL(client_.GetJobClientProvider(), PutJob)
      .WillOnce([=](auto& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  atomic_bool finished = false;
  auto context = AsyncContext<PutJobRequest, PutJobResponse>(
      make_shared<PutJobRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.PutJob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, PutJobSyncSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), PutJob)
      .WillOnce([=](AsyncContext<PutJobRequest, PutJobResponse>& context) {
        context.response = make_shared<PutJobResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(client_.PutJobSync(PutJobRequest()).result());
}

TEST_F(JobClientTest, GetNextJobSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), GetNextJob)
      .WillOnce([=](auto& context) {
        context.response = make_shared<GetNextJobResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<GetNextJobRequest, GetNextJobResponse>(
      make_shared<GetNextJobRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(GetNextJobResponse()));
        finished = true;
      });
  client_.GetNextJob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, GetNextJobFailure) {
  EXPECT_CALL(client_.GetJobClientProvider(), GetNextJob)
      .WillOnce([=](auto& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  atomic_bool finished = false;
  auto context = AsyncContext<GetNextJobRequest, GetNextJobResponse>(
      make_shared<GetNextJobRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.GetNextJob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, GetNextJobSyncSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), GetNextJob)
      .WillOnce(
          [=](AsyncContext<GetNextJobRequest, GetNextJobResponse>& context) {
            context.response = make_shared<GetNextJobResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_.GetNextJobSync(GetNextJobRequest()).result());
}

TEST_F(JobClientTest, GetJobByIdSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), GetJobById)
      .WillOnce([=](auto& context) {
        context.response = make_shared<GetJobByIdResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<GetJobByIdRequest, GetJobByIdResponse>(
      make_shared<GetJobByIdRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(GetJobByIdResponse()));
        finished = true;
      });
  client_.GetJobById(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, GetJobByIdFailure) {
  EXPECT_CALL(client_.GetJobClientProvider(), GetJobById)
      .WillOnce([=](auto& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  atomic_bool finished = false;
  auto context = AsyncContext<GetJobByIdRequest, GetJobByIdResponse>(
      make_shared<GetJobByIdRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.GetJobById(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, GetJobByIdSyncSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), GetJobById)
      .WillOnce(
          [=](AsyncContext<GetJobByIdRequest, GetJobByIdResponse>& context) {
            context.response = make_shared<GetJobByIdResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_.GetJobByIdSync(GetJobByIdRequest()).result());
}

TEST_F(JobClientTest, UpdateJobBodySuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobBody)
      .WillOnce([=](auto& context) {
        context.response = make_shared<UpdateJobBodyResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse>(
      make_shared<UpdateJobBodyRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(UpdateJobBodyResponse()));
        finished = true;
      });
  client_.UpdateJobBody(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, UpdateJobBodyFailure) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobBody)
      .WillOnce([=](auto& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  atomic_bool finished = false;
  auto context = AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse>(
      make_shared<UpdateJobBodyRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.UpdateJobBody(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, UpdateJobBodySyncSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobBody)
      .WillOnce([=](AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse>&
                        context) {
        context.response = make_shared<UpdateJobBodyResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(client_.UpdateJobBodySync(UpdateJobBodyRequest()).result());
}

TEST_F(JobClientTest, UpdateJobStatusSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobStatus)
      .WillOnce([=](auto& context) {
        context.response = make_shared<UpdateJobStatusResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>(
      make_shared<UpdateJobStatusRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(UpdateJobStatusResponse()));
        finished = true;
      });
  client_.UpdateJobStatus(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, UpdateJobStatusFailure) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobStatus)
      .WillOnce([=](auto& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  atomic_bool finished = false;
  auto context = AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>(
      make_shared<UpdateJobStatusRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.UpdateJobStatus(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, UpdateJobStatusSyncSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobStatus)
      .WillOnce([=](AsyncContext<UpdateJobStatusRequest,
                                 UpdateJobStatusResponse>& context) {
        context.response = make_shared<UpdateJobStatusResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(
      client_.UpdateJobStatusSync(UpdateJobStatusRequest()).result());
}

TEST_F(JobClientTest, UpdateJobVisibilityTimeout) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobVisibilityTimeout)
      .WillOnce([=](auto& context) {
        context.response = make_shared<UpdateJobVisibilityTimeoutResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<UpdateJobVisibilityTimeoutRequest,
                              UpdateJobVisibilityTimeoutResponse>(
      make_shared<UpdateJobVisibilityTimeoutRequest>(),
      [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response,
                    EqualsProto(UpdateJobVisibilityTimeoutResponse()));
        finished = true;
      });
  client_.UpdateJobVisibilityTimeout(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, UpdateJobVisibilityTimeoutFailure) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobVisibilityTimeout)
      .WillOnce([=](auto& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  atomic_bool finished = false;
  auto context = AsyncContext<UpdateJobVisibilityTimeoutRequest,
                              UpdateJobVisibilityTimeoutResponse>(
      make_shared<UpdateJobVisibilityTimeoutRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.UpdateJobVisibilityTimeout(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, UpdateJobVisibilityTimeoutSyncSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobVisibilityTimeout)
      .WillOnce([=](AsyncContext<UpdateJobVisibilityTimeoutRequest,
                                 UpdateJobVisibilityTimeoutResponse>& context) {
        context.response = make_shared<UpdateJobVisibilityTimeoutResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(
      client_
          .UpdateJobVisibilityTimeoutSync(UpdateJobVisibilityTimeoutRequest())
          .result());
}

TEST_F(JobClientTest, DeleteOrphanedJobMessage) {
  EXPECT_CALL(client_.GetJobClientProvider(), DeleteOrphanedJobMessage)
      .WillOnce([=](auto& context) {
        context.response = make_shared<DeleteOrphanedJobMessageResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<DeleteOrphanedJobMessageRequest,
                              DeleteOrphanedJobMessageResponse>(
      make_shared<DeleteOrphanedJobMessageRequest>(),
      [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response,
                    EqualsProto(DeleteOrphanedJobMessageResponse()));
        finished = true;
      });
  client_.DeleteOrphanedJobMessage(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, DeleteOrphanedJobMessageFailure) {
  EXPECT_CALL(client_.GetJobClientProvider(), DeleteOrphanedJobMessage)
      .WillOnce([=](auto& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  atomic_bool finished = false;
  auto context = AsyncContext<DeleteOrphanedJobMessageRequest,
                              DeleteOrphanedJobMessageResponse>(
      make_shared<DeleteOrphanedJobMessageRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.DeleteOrphanedJobMessage(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(JobClientTest, DeleteOrphanedJobMessageSyncSuccess) {
  EXPECT_CALL(client_.GetJobClientProvider(), DeleteOrphanedJobMessage)
      .WillOnce([=](AsyncContext<DeleteOrphanedJobMessageRequest,
                                 DeleteOrphanedJobMessageResponse>& context) {
        context.response = make_shared<DeleteOrphanedJobMessageResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(
      client_.DeleteOrphanedJobMessageSync(DeleteOrphanedJobMessageRequest())
          .result());
}
}  // namespace google::scp::cpio::test
