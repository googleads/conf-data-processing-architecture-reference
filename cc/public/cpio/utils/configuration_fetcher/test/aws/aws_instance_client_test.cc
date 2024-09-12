// Copyright 2024 Google LLC
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

#include "public/cpio/utils/configuration_fetcher/src/aws/aws_instance_client.h"

#include <memory>

#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::scp::core::AsyncContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;

namespace google::scp::cpio::test {
TEST(AwsInstanceClientTest, InstanceClientProviderIsPassedIn) {
  auto instance_client_provider = make_shared<MockInstanceClientProvider>();

  EXPECT_CALL(*instance_client_provider, GetCurrentInstanceResourceName)
      .WillOnce([](auto& context) {
        context.response =
            make_shared<GetCurrentInstanceResourceNameResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
      });
  auto client = make_unique<AwsInstanceClient>(
      make_shared<InstanceClientOptions>(), instance_client_provider);
  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

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

  client->GetCurrentInstanceResourceName(context);
  WaitUntil([&]() { return finished.load(); });

  EXPECT_SUCCESS(client->Stop());
}
}  // namespace google::scp::cpio::test
