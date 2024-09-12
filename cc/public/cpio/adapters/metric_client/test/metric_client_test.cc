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

#include "public/cpio/adapters/metric_client/src/metric_client.h"

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <utility>

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/metric_client/mock/mock_metric_client_with_overrides.h"
#include "public/cpio/interface/error_codes.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
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
using google::scp::cpio::MetricClient;
using google::scp::cpio::MetricClientOptions;
using google::scp::cpio::mock::MockMetricClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Return;

namespace google::scp::cpio::test {
class MetricClientTest : public ScpTestBase {
 protected:
  MetricClientTest() {
    auto metric_client_options = make_shared<MetricClientOptions>();
    client_ = make_unique<MockMetricClientWithOverrides>(metric_client_options);

    EXPECT_THAT(client_->Init(), IsSuccessful());
    EXPECT_THAT(client_->Run(), IsSuccessful());
  }

  ~MetricClientTest() { EXPECT_THAT(client_->Stop(), IsSuccessful()); }

  unique_ptr<MockMetricClientWithOverrides> client_;
};

TEST_F(MetricClientTest, PutMetricsSuccess) {
  EXPECT_CALL(*client_->GetMetricClientProvider(), PutMetrics)
      .WillOnce([=](auto& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<PutMetricsResponse>();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<PutMetricsRequest, PutMetricsResponse>(
      make_shared<PutMetricsRequest>(), [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(PutMetricsResponse()));
        finished = true;
      });

  client_->PutMetrics(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(MetricClientTest, PutMetricsSyncSuccess) {
  EXPECT_CALL(*client_->GetMetricClientProvider(), PutMetrics)
      .WillOnce(
          [=](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
            context.response = make_shared<PutMetricsResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_->PutMetricsSync(PutMetricsRequest()).result());
}

TEST_F(MetricClientTest, FailureToCreateMetricClientProvider) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->create_metric_client_provider_result = failure;
  EXPECT_EQ(client_->Init(), FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR));
}
}  // namespace google::scp::cpio::test
