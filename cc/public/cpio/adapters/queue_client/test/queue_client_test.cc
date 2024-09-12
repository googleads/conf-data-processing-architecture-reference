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

#include "public/cpio/adapters/queue_client/src/queue_client.h"

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/queue_client/mock/mock_queue_client_with_overrides.h"
#include "public/cpio/interface/queue_client/queue_client_interface.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

using google::cmrt::sdk::queue_service::v1::DeleteMessageRequest;
using google::cmrt::sdk::queue_service::v1::DeleteMessageResponse;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageRequest;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageResponse;
using google::cmrt::sdk::queue_service::v1::GetTopMessageRequest;
using google::cmrt::sdk::queue_service::v1::GetTopMessageResponse;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutRequest;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutResponse;
using google::scp::core::AsyncContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::mock::MockQueueClientWithOverrides;
using std::atomic;
using std::make_shared;
using testing::Return;

namespace google::scp::cpio::test {

class QueueClientTest : public ScpTestBase {
 protected:
  QueueClientTest() { assert(client_.Init().Successful()); }

  MockQueueClientWithOverrides client_;
};

TEST_F(QueueClientTest, EnqueueMessageSuccess) {
  EXPECT_CALL(client_.GetQueueClientProvider(), EnqueueMessage)
      .WillOnce([=](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                        context) {
        context.response = make_shared<EnqueueMessageResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>(
      make_shared<EnqueueMessageRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(EnqueueMessageResponse()));
        finished = true;
      });
  client_.EnqueueMessage(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(QueueClientTest, EnqueueMessageSyncSuccess) {
  EXPECT_CALL(client_.GetQueueClientProvider(), EnqueueMessage)
      .WillOnce([=](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                        context) {
        context.response = make_shared<EnqueueMessageResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(client_.EnqueueMessageSync(EnqueueMessageRequest()).result());
}

TEST_F(QueueClientTest, GetTopMessageSuccess) {
  EXPECT_CALL(client_.GetQueueClientProvider(), GetTopMessage)
      .WillOnce([=](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                        context) {
        context.response = make_shared<GetTopMessageResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<GetTopMessageRequest, GetTopMessageResponse>(
      make_shared<GetTopMessageRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(GetTopMessageResponse()));
        finished = true;
      });
  client_.GetTopMessage(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(QueueClientTest, GetTopMessageSyncSuccess) {
  EXPECT_CALL(client_.GetQueueClientProvider(), GetTopMessage)
      .WillOnce([=](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                        context) {
        context.response = make_shared<GetTopMessageResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(client_.GetTopMessageSync(GetTopMessageRequest()).result());
}

TEST_F(QueueClientTest, UpdateMessageVisibilityTimeoutSuccess) {
  EXPECT_CALL(client_.GetQueueClientProvider(), UpdateMessageVisibilityTimeout)
      .WillOnce(
          [=](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                           UpdateMessageVisibilityTimeoutResponse>& context) {
            context.response =
                make_shared<UpdateMessageVisibilityTimeoutResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });

  atomic<bool> finished = false;
  auto context = AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                              UpdateMessageVisibilityTimeoutResponse>(
      make_shared<UpdateMessageVisibilityTimeoutRequest>(),
      [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response,
                    EqualsProto(UpdateMessageVisibilityTimeoutResponse()));
        finished = true;
      });
  client_.UpdateMessageVisibilityTimeout(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(QueueClientTest, UpdateMessageVisibilityTimeoutSyncSuccess) {
  EXPECT_CALL(client_.GetQueueClientProvider(), UpdateMessageVisibilityTimeout)
      .WillOnce(
          [=](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                           UpdateMessageVisibilityTimeoutResponse>& context) {
            context.response =
                make_shared<UpdateMessageVisibilityTimeoutResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_
                     .UpdateMessageVisibilityTimeoutSync(
                         UpdateMessageVisibilityTimeoutRequest())
                     .result());
}

TEST_F(QueueClientTest, DeleteMessageSuccess) {
  EXPECT_CALL(client_.GetQueueClientProvider(), DeleteMessage)
      .WillOnce([=](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                        context) {
        context.response = make_shared<DeleteMessageResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<DeleteMessageRequest, DeleteMessageResponse>(
      make_shared<DeleteMessageRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(DeleteMessageResponse()));
        finished = true;
      });
  client_.DeleteMessage(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(QueueClientTest, DeleteMessageSyncSuccess) {
  EXPECT_CALL(client_.GetQueueClientProvider(), DeleteMessage)
      .WillOnce([=](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                        context) {
        context.response = make_shared<DeleteMessageResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(client_.DeleteMessageSync(DeleteMessageRequest()).result());
}
}  // namespace google::scp::cpio::test
