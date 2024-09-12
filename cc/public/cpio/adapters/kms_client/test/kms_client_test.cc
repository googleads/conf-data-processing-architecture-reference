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

#include "public/cpio/adapters/kms_client/src/kms_client.h"

#include <atomic>

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/kms_client/mock/mock_kms_client_with_overrides.h"
#include "public/cpio/interface/kms_client/kms_client_interface.h"
#include "public/cpio/proto/kms_service/v1/kms_service.pb.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::mock::MockKmsClientWithOverrides;
using std::atomic_bool;
using std::make_shared;

namespace google::scp::cpio::test {

class KmsClientTest : public ScpTestBase {
 protected:
  KmsClientTest() { assert(client_.Init().Successful()); }

  MockKmsClientWithOverrides client_;
};

TEST_F(KmsClientTest, DecryptSuccess) {
  auto& client_provider = client_.GetKmsClientProvider();
  atomic_bool finished(false);
  EXPECT_CALL(client_provider, Decrypt).WillOnce([=](auto context) {
    context.result = SuccessExecutionResult();
    context.response = make_shared<DecryptResponse>();
    context.Finish();
  });

  auto context = AsyncContext<DecryptRequest, DecryptResponse>(
      make_shared<DecryptRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(DecryptResponse()));
        finished = true;
      });

  client_.Decrypt(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(KmsClientTest, DecryptSyncSuccess) {
  EXPECT_CALL(client_.GetKmsClientProvider(), Decrypt)
      .WillOnce([=](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        context.response = make_shared<DecryptResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(client_.DecryptSync(DecryptRequest()).result());
}
}  // namespace google::scp::cpio::test
