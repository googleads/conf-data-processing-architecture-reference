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

#include "public/cpio/adapters/auto_scaling_client/src/auto_scaling_client.h"

#include <gmock/gmock.h>

#include <memory>
#include <string>

#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/auto_scaling_client/mock/mock_auto_scaling_client_with_overrides.h"
#include "public/cpio/core/mock/mock_lib_cpio.h"
#include "public/cpio/interface/auto_scaling_client/auto_scaling_client_interface.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CPIO_UNKNOWN_ERROR;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::mock::MockAutoScalingClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Return;

namespace google::scp::cpio::test {
class AutoScalingClientTest : public ScpTestBase {
 protected:
  AutoScalingClientTest() {
    auto auto_scaling_client_options = make_shared<AutoScalingClientOptions>();
    client_ = make_unique<MockAutoScalingClientWithOverrides>(
        auto_scaling_client_options);

    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());
  }

  ~AutoScalingClientTest() { EXPECT_SUCCESS(client_->Stop()); }

  unique_ptr<MockAutoScalingClientWithOverrides> client_;
};

TEST_F(AutoScalingClientTest, TryFinishInstanceTerminationSuccess) {
  EXPECT_CALL(*client_->GetAutoScalingClientProvider(),
              TryFinishInstanceTermination)
      .WillOnce(
          [=](AsyncContext<TryFinishInstanceTerminationRequest,
                           TryFinishInstanceTerminationResponse>& context) {
            context.result = SuccessExecutionResult();
            context.response =
                make_shared<TryFinishInstanceTerminationResponse>();
            context.Finish();
          });

  atomic<bool> finished = false;
  auto context = AsyncContext<TryFinishInstanceTerminationRequest,
                              TryFinishInstanceTerminationResponse>(
      make_shared<TryFinishInstanceTerminationRequest>(), [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response,
                    EqualsProto(TryFinishInstanceTerminationResponse()));
        finished = true;
      });

  client_->TryFinishInstanceTermination(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(AutoScalingClientTest, TryFinishInstanceTerminationSyncSuccess) {
  EXPECT_CALL(*client_->GetAutoScalingClientProvider(),
              TryFinishInstanceTermination)
      .WillOnce(
          [=](AsyncContext<TryFinishInstanceTerminationRequest,
                           TryFinishInstanceTerminationResponse>& context) {
            context.response =
                make_shared<TryFinishInstanceTerminationResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_
                     ->TryFinishInstanceTerminationSync(
                         TryFinishInstanceTerminationRequest())
                     .result());
}

TEST_F(AutoScalingClientTest, TryFinishInstanceTerminationFailure) {
  EXPECT_CALL(*client_->GetAutoScalingClientProvider(),
              TryFinishInstanceTermination)
      .WillOnce(
          [=](AsyncContext<TryFinishInstanceTerminationRequest,
                           TryFinishInstanceTerminationResponse>& context) {
            context.result = FailureExecutionResult(SC_UNKNOWN);
            context.Finish();
            return FailureExecutionResult(SC_UNKNOWN);
          });

  atomic<bool> finished = false;
  auto context = AsyncContext<TryFinishInstanceTerminationRequest,
                              TryFinishInstanceTerminationResponse>(
      make_shared<TryFinishInstanceTerminationRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_->TryFinishInstanceTermination(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(AutoScalingClientTest, TryFinishInstanceTerminationSyncFailure1) {
  EXPECT_CALL(*client_->GetAutoScalingClientProvider(),
              TryFinishInstanceTermination)
      .WillOnce(
          [=](AsyncContext<TryFinishInstanceTerminationRequest,
                           TryFinishInstanceTerminationResponse>& context) {
            context.result = FailureExecutionResult(SC_UNKNOWN);
            context.Finish();
            return SuccessExecutionResult();
          });
  client_
      ->TryFinishInstanceTerminationSync(TryFinishInstanceTerminationRequest())
      .result(),
      ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR));
}

TEST_F(AutoScalingClientTest, FailureToCreateAutoScalingClient) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->create_auto_scaling_client_provider_result = failure;
  EXPECT_EQ(client_->Init(), FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR));
}

TEST(AutoScalingClientInitTest, FailureToRun) {
  auto auto_scaling_client_options = make_shared<AutoScalingClientOptions>();
  auto client = make_unique<MockAutoScalingClientWithOverrides>(
      auto_scaling_client_options);

  EXPECT_SUCCESS(client->Init());
  EXPECT_CALL(*client->GetAutoScalingClientProvider(), Run)
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_EQ(client->Run(), FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR));
}

TEST(AutoScalingClientInitTest, FailureToStop) {
  auto auto_scaling_client_options = make_shared<AutoScalingClientOptions>();
  auto client = make_unique<MockAutoScalingClientWithOverrides>(
      auto_scaling_client_options);

  EXPECT_SUCCESS(client->Init());
  EXPECT_CALL(*client->GetAutoScalingClientProvider(), Stop)
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(client->Run());
  EXPECT_EQ(client->Stop(), FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR));
}
}  // namespace google::scp::cpio::test
