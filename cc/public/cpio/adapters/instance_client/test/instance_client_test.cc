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

#include "public/cpio/adapters/instance_client/src/instance_client.h"

#include <gmock/gmock.h>

#include <memory>
#include <string>

#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/instance_client/mock/mock_instance_client_with_overrides.h"
#include "public/cpio/core/mock/mock_lib_cpio.h"
#include "public/cpio/interface/instance_client/instance_client_interface.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
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
using google::scp::cpio::mock::MockInstanceClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Return;

namespace google::scp::cpio::test {
class InstanceClientTest : public ScpTestBase {
 protected:
  InstanceClientTest() {
    auto instance_client_options = make_shared<InstanceClientOptions>();
    client_ =
        make_unique<MockInstanceClientWithOverrides>(instance_client_options);

    EXPECT_THAT(client_->Init(), IsSuccessful());
    EXPECT_THAT(client_->Run(), IsSuccessful());
  }

  ~InstanceClientTest() { EXPECT_THAT(client_->Stop(), IsSuccessful()); }

  unique_ptr<MockInstanceClientWithOverrides> client_;
};

TEST_F(InstanceClientTest, GetCurrentInstanceResourceNameSuccess) {
  EXPECT_CALL(*client_->GetInstanceClientProvider(),
              GetCurrentInstanceResourceName)
      .WillOnce([](auto& context) {
        context.response =
            make_shared<GetCurrentInstanceResourceNameResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<GetCurrentInstanceResourceNameRequest,
                              GetCurrentInstanceResourceNameResponse>(
      make_shared<GetCurrentInstanceResourceNameRequest>(),
      [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response,
                    EqualsProto(GetCurrentInstanceResourceNameResponse()));
        finished = true;
      });

  client_->GetCurrentInstanceResourceName(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(InstanceClientTest, GetCurrentInstanceResourceNameSyncSuccess) {
  EXPECT_CALL(*client_->GetInstanceClientProvider(),
              GetCurrentInstanceResourceName)
      .WillOnce(
          [=](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            context.response =
                make_shared<GetCurrentInstanceResourceNameResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_
                     ->GetCurrentInstanceResourceNameSync(
                         GetCurrentInstanceResourceNameRequest())
                     .result());
}

TEST_F(InstanceClientTest, GetTagsByResourceNameSuccess) {
  EXPECT_CALL(*client_->GetInstanceClientProvider(), GetTagsByResourceName)
      .WillOnce([](auto& context) {
        context.response = make_shared<GetTagsByResourceNameResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context =
      AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>(
          make_shared<GetTagsByResourceNameRequest>(),
          [&finished](auto& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_THAT(*context.response,
                        EqualsProto(GetTagsByResourceNameResponse()));
            finished = true;
          });

  client_->GetTagsByResourceName(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(InstanceClientTest, GetTagsByResourceNameSyncSuccess) {
  EXPECT_CALL(*client_->GetInstanceClientProvider(), GetTagsByResourceName)
      .WillOnce([=](AsyncContext<GetTagsByResourceNameRequest,
                                 GetTagsByResourceNameResponse>& context) {
        context.response = make_shared<GetTagsByResourceNameResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(
      client_->GetTagsByResourceNameSync(GetTagsByResourceNameRequest())
          .result());
}

TEST_F(InstanceClientTest, GetInstanceDetailsByResourceNameSuccess) {
  EXPECT_CALL(*client_->GetInstanceClientProvider(),
              GetInstanceDetailsByResourceName)
      .WillOnce([](auto& context) {
        context.response =
            make_shared<GetInstanceDetailsByResourceNameResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<GetInstanceDetailsByResourceNameRequest,
                              GetInstanceDetailsByResourceNameResponse>(
      make_shared<GetInstanceDetailsByResourceNameRequest>(),
      [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response,
                    EqualsProto(GetInstanceDetailsByResourceNameResponse()));
        finished = true;
      });

  client_->GetInstanceDetailsByResourceName(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(InstanceClientTest, GetInstanceDetailsByResourceNameSyncSuccess) {
  EXPECT_CALL(*client_->GetInstanceClientProvider(),
              GetInstanceDetailsByResourceName)
      .WillOnce(
          [=](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            context.response =
                make_shared<GetInstanceDetailsByResourceNameResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_
                     ->GetInstanceDetailsByResourceNameSync(
                         GetInstanceDetailsByResourceNameRequest())
                     .result());
}

TEST_F(InstanceClientTest, FailureToCreateInstanceClientProvider) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->create_instance_client_provider_result = failure;
  EXPECT_EQ(client_->Init(), FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR));
}
}  // namespace google::scp::cpio::test
