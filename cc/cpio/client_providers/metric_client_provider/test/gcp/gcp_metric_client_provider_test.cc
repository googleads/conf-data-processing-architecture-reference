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

#include "cc/cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_provider.h"

#include <memory>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/strings/str_cat.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/scp_test_base.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/mock/gcp/mock_gcp_metric_client_provider_with_overrides.h"
#include "cpio/client_providers/metric_client_provider/src/gcp/error_codes.h"
#include "cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_utils.h"
#include "cpio/common/src/gcp/error_codes.h"
#include "google/cloud/monitoring/mocks/mock_metric_connection.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using absl::StrCat;
using google::cloud::make_ready_future;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::monitoring::MetricServiceClient;
using google::cloud::monitoring_mocks::MockMetricServiceConnection;
using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::monitoring::v3::CreateTimeSeriesRequest;
using google::protobuf::Timestamp;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::cpio::client_providers::GcpMetricClientUtils;
using google::scp::cpio::client_providers::mock::
    MockGcpMetricClientProviderOverrides;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::stod;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;

namespace {
constexpr char kName[] = "test_name";
constexpr char kValue[] = "12346.89";
constexpr char kNamespace[] = "gcp_namespace";
constexpr char kDifferentNamespace[] = "different_namespace";
constexpr char kProjectIdValue[] = "123456789";
constexpr char kInstanceIdValue[] = "987654321";
constexpr char kInstanceZoneValue[] = "us-central1-c";

constexpr char kInstanceResourceName[] =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";

constexpr char kResourceType[] = "gce_instance";
constexpr char kProjectIdKey[] = "project_id";
constexpr char kInstanceIdKey[] = "instance_id";
constexpr char kInstanceZoneKey[] = "zone";

void SetPutMetricsRequest(
    PutMetricsRequest& record_metric_request, const string& value = kValue,
    const int64_t& timestamp_in_ms =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count(),
    int number_of_metrics = 1) {
  for (auto i = 0; i < number_of_metrics; i++) {
    auto metric = record_metric_request.add_metrics();
    auto name = i > 0 ? StrCat(kName, to_string(i)) : kName;
    metric->set_name(name);
    metric->set_value(value);
    *metric->mutable_timestamp() =
        TimeUtil::MillisecondsToTimestamp(timestamp_in_ms);
  }
}

}  // namespace

namespace google::scp::cpio::client_providers::gcp_metric_client::test {

class MockGcpMetricServiceClientFactory : public GcpMetricServiceClientFactory {
 public:
  MOCK_METHOD(shared_ptr<MetricServiceClient>, CreateClient, (),
              (noexcept, override));
};

class GcpMetricClientProviderTest : public ScpTestBase {
 protected:
  void SetUp() override {
    async_executor_mock_ = make_shared<MockAsyncExecutor>();
    async_executor_mock_->schedule_for_mock =
        [&](const core::AsyncOperation& work, Timestamp timestamp,
            std::function<bool()>& cancellation_callback) {
          return core::SuccessExecutionResult();
        };

    instance_client_provider_mock_ = make_shared<MockInstanceClientProvider>();
    instance_client_provider_mock_->instance_resource_name =
        kInstanceResourceName;

    mock_factory_ = make_shared<MockGcpMetricServiceClientFactory>();
    connection_ = make_shared<NiceMock<MockMetricServiceConnection>>();
    mock_client_ = make_shared<MetricServiceClient>(connection_);
    ON_CALL(*mock_factory_, CreateClient).WillByDefault(Return(mock_client_));

    metric_client_provider_ = CreateClient(false);
    EXPECT_SUCCESS(metric_client_provider_->Init());
    EXPECT_SUCCESS(metric_client_provider_->Run());
  }

  unique_ptr<MockGcpMetricClientProviderOverrides> CreateClient(
      bool enable_batch_recording) {
    auto metric_client_options = make_shared<MetricClientOptions>();
    metric_client_options->enable_batch_recording = enable_batch_recording;
    if (enable_batch_recording) {
      metric_client_options->namespace_for_batch_recording = kNamespace;
    }
    return make_unique<MockGcpMetricClientProviderOverrides>(
        metric_client_options, instance_client_provider_mock_, mock_factory_,
        async_executor_mock_);
  }

  shared_ptr<MockGcpMetricServiceClientFactory> mock_factory_;
  shared_ptr<MockAsyncExecutor> async_executor_mock_;
  shared_ptr<MockInstanceClientProvider> instance_client_provider_mock_;
  shared_ptr<MetricServiceClient> mock_client_;
  shared_ptr<MockMetricServiceConnection> connection_;
  unique_ptr<MockGcpMetricClientProviderOverrides> metric_client_provider_;
};

MATCHER_P2(RequestEquals, metric_name, metric_namespace, "") {
  bool equal = true;
  if (arg.name() != metric_name) {
    equal = false;
  }
  if (arg.time_series()[0].metric().type() != "custom.googleapis.com/" +
                                                  string(metric_namespace) +
                                                  "/" + string(kName)) {
    equal = false;
  }
  if (arg.time_series()[0].points()[0].value().double_value() != stod(kValue)) {
    equal = false;
  }
  auto resource = arg.time_series()[0].resource();
  if (resource.type() != kResourceType) {
    equal = false;
  }
  if (resource.labels().find(kProjectIdKey)->second != kProjectIdValue) {
    equal = false;
  }
  if (resource.labels().find(kInstanceIdKey)->second != kInstanceIdValue) {
    equal = false;
  }
  if (resource.labels().find(kInstanceZoneKey)->second != kInstanceZoneValue) {
    equal = false;
  }
  return equal;
}

TEST_F(GcpMetricClientProviderTest,
       UseNamespaceFromRequestWithoutBatchRecording) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  record_metric_request.set_metric_namespace(kDifferentNamespace);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
  auto requests_vector = make_shared<
      vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();

  auto metric_name =
      GcpMetricClientUtils::ConstructProjectName(kProjectIdValue);
  atomic<int> received_metrics = 0;
  EXPECT_CALL(*connection_, AsyncCreateTimeSeries(RequestEquals(
                                metric_name, kDifferentNamespace)))
      .WillRepeatedly([&](CreateTimeSeriesRequest const& request) {
        received_metrics.fetch_add(request.time_series().size());
        return make_ready_future(Status(StatusCode::kOk, ""));
      });

  for (auto i = 0; i < 5; i++) {
    requests_vector->emplace_back(context);
  }
  auto result = metric_client_provider_->MetricsBatchPush(requests_vector);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(received_metrics, 5);
}

TEST_F(GcpMetricClientProviderTest,
       MetricsBatchPushShouldGroupContextsMetrics) {
  metric_client_provider_ = CreateClient(true);
  EXPECT_SUCCESS(metric_client_provider_->Init());
  EXPECT_SUCCESS(metric_client_provider_->Run());

  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
  auto requests_vector = make_shared<
      vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();

  auto metric_name =
      GcpMetricClientUtils::ConstructProjectName(kProjectIdValue);
  atomic<int> received_metrics = 0;
  EXPECT_CALL(*connection_,
              AsyncCreateTimeSeries(RequestEquals(metric_name, kNamespace)))
      .WillOnce([&](CreateTimeSeriesRequest const& request) {
        received_metrics.fetch_add(request.time_series().size());
        return make_ready_future(Status(StatusCode::kOk, ""));
      });

  for (auto i = 0; i < 200; i++) {
    requests_vector->emplace_back(context);
  }
  auto result = metric_client_provider_->MetricsBatchPush(requests_vector);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(received_metrics, 200);
}

TEST_F(GcpMetricClientProviderTest, ShouldNotGroupContextsIfMetricsOversize) {
  metric_client_provider_ = CreateClient(true);
  EXPECT_SUCCESS(metric_client_provider_->Init());
  EXPECT_SUCCESS(metric_client_provider_->Run());

  PutMetricsRequest record_metric_request;
  auto timestamp_in_ms =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();
  SetPutMetricsRequest(record_metric_request, kValue, timestamp_in_ms,
                       150 /*number of metrics in the request*/);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
  auto requests_vector = make_shared<
      vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();

  auto metric_name =
      GcpMetricClientUtils::ConstructProjectName(kProjectIdValue);
  atomic<int> received_metrics = 0;
  EXPECT_CALL(*connection_,
              AsyncCreateTimeSeries(RequestEquals(metric_name, kNamespace)))
      .Times(5)
      .WillRepeatedly([&](CreateTimeSeriesRequest const& request) {
        received_metrics.fetch_add(request.time_series().size());
        return make_ready_future(Status(StatusCode::kOk, ""));
      });
  int number_of_context(5);
  for (auto i = 0; i < number_of_context; i++) {
    requests_vector->emplace_back(context);
  }
  auto result = metric_client_provider_->MetricsBatchPush(requests_vector);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(received_metrics, 150 * number_of_context);
}

TEST_F(GcpMetricClientProviderTest, FailedMetricsBatchPush) {
  metric_client_provider_ = CreateClient(true);
  EXPECT_SUCCESS(metric_client_provider_->Init());
  EXPECT_SUCCESS(metric_client_provider_->Run());

  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  atomic<int> metric_responses = 0;
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        metric_responses++;
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_INVALID_ARGUMENT)));
      });
  auto requests_vector = make_shared<
      vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();

  auto metric_name =
      GcpMetricClientUtils::ConstructProjectName(kProjectIdValue);
  atomic<int> received_metrics = 0;
  EXPECT_CALL(*connection_,
              AsyncCreateTimeSeries(RequestEquals(metric_name, kNamespace)))
      .WillRepeatedly([&](CreateTimeSeriesRequest const& request) {
        received_metrics.fetch_add(request.time_series().size());
        return make_ready_future(
            Status(StatusCode::kInvalidArgument, "Error Not Found"));
      });

  for (auto i = 0; i < 5; i++) {
    requests_vector->emplace_back(context);
  }
  auto result = metric_client_provider_->MetricsBatchPush(requests_vector);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(metric_responses, 5);
  EXPECT_EQ(received_metrics, 5);
}

TEST_F(GcpMetricClientProviderTest, AsyncCreateTimeSeriesCallback) {
  atomic<int> received_responses = 0;
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        received_responses++;
        EXPECT_SUCCESS(context.result);
      });

  vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>> requests_vector;
  for (auto i = 0; i < 5; i++) {
    requests_vector.emplace_back(context);
  }

  auto outcome = make_ready_future(Status(StatusCode::kOk, ""));

  metric_client_provider_->OnAsyncCreateTimeSeriesCallback(
      make_shared<vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>(
          requests_vector),
      move(outcome));
  EXPECT_EQ(received_responses, 5);
}

}  // namespace google::scp::cpio::client_providers::gcp_metric_client::test
