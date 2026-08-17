// Copyright 2023 Google LLC
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

#include "public/cpio/utils/configuration_fetcher/src/configuration_fetcher.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/scp_test_base.h"
#include "cpio/server/interface/configuration_keys.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/mock/instance_client/mock_instance_client.h"
#include "public/cpio/mock/parameter_client/mock_parameter_client.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"
#include "public/cpio/proto/auto_scaling_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/common/v1/common_configuration_keys.pb.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"
#include "public/cpio/proto/job_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/metric_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/nosql_database_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "public/cpio/proto/queue_service/v1/configuration_keys.pb.h"
#include "public/cpio/utils/configuration_fetcher/src/error_codes.h"
#include "public/cpio/utils/job_lifecycle_helper/proto/v1/configuration_keys.pb.h"
#include "public/cpio/utils/job_lifecycle_helper/proto/v1/job_lifecycle_helper.pb.h"

using google::scp::core::test::ScpTestBase;
namespace QueueClientProto = google::cmrt::sdk::queue_service::v1;
namespace NoSQLDatabaseClientProto =
    google::cmrt::sdk::nosql_database_service::v1;
namespace AutoScalingClientProto = google::cmrt::sdk::auto_scaling_service::v1;
namespace MetricClientProto = google::cmrt::sdk::metric_service::v1;
namespace JobClientProto = google::cmrt::sdk::job_service::v1;
namespace JobLifecycleHelperProto = google::cmrt::sdk::job_lifecycle_helper::v1;
using google::cmrt::sdk::common::v1::CommonClientConfigurationKeys;
using google::cmrt::sdk::common::v1::CommonClientConfigurationKeys_Name;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::protobuf::MapPair;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LogLevel;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_INSTANCE_RESOURCE_NAME_NOT_FOUND;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_INVALID_ENVIRONMENT_NAME_LABEL;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_INVALID_PARAMETER_NAME;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::nullopt;
using std::optional;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_set;
using std::vector;
using testing::NiceMock;
using testing::UnorderedElementsAre;

namespace {
constexpr char kInstanceResourceName[] =
    "projects/123/zones/us-central-1/instances/345";
constexpr char kEnvNameLabel[] = "environment";
constexpr char kEnvName[] = "test";
constexpr char kTestTable[] = "test-table";
constexpr char kTestQueue[] = "test-queue";
constexpr char kTestGcpSpannerInstance[] = "test-spannner-instance";
constexpr char kTestGcpSpannerDatabase[] = "test-spannner-database";
constexpr char kTestCommonThreadCount[] = "10";
constexpr char kTestCommonThreadPoolQueueCap[] = "10000";
constexpr char kTestLogOption[] = "ConsoleLog";
constexpr char kTestMetricNamespace[] = "metric_namespace";
constexpr char kTestMetricOpenTelemetryCollectorAddress[] =
    "collector.hostname.google.com";
constexpr char kTestLogLevels[] = "Debug,Info";
constexpr char kTestRetryInterval[] = "123456";
constexpr char kTestRetryLimit[] = "3";
constexpr char kTestVisibilityTimeoutExtendTime[] = "30";
constexpr char kTestJobProcessingTimeout[] = "120";
constexpr char kTestJobExtendingWorkerSleepTime[] = "60";
constexpr char kTestInstanceTableName[] = "instance-name";
constexpr char kTestScaleInHookName[] = "scale-in-hook";
}  // namespace

namespace google::scp::cpio {
class MockConfigurationFetcherWithOverrides : public ConfigurationFetcher {
 public:
  MockConfigurationFetcherWithOverrides(
      const std::shared_ptr<MockInstanceClient>& instance_client,
      const std::shared_ptr<MockParameterClient>& parameter_client,
      std::optional<std::string> parameter_name_prefix,
      std::optional<std::string> environment_name_label)
      : ConfigurationFetcher(parameter_name_prefix, environment_name_label),
        mock_instance_client_(instance_client),
        mock_parameter_client_(parameter_client) {}

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

 protected:
  void CreateInstanceAndParameterClient() noexcept override {
    instance_client_ = mock_instance_client_;
    parameter_client_ = mock_parameter_client_;
  }

  ExecutionResult InitDependencies() noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult RunDependencies() noexcept override {
    return SuccessExecutionResult();
  }

 private:
  std::shared_ptr<MockInstanceClient> mock_instance_client_;
  std::shared_ptr<MockParameterClient> mock_parameter_client_;
};

class ConfigurationFetcherTest : public ScpTestBase {
 protected:
  void SetUp() override {
    mock_instance_client_ = make_shared<NiceMock<MockInstanceClient>>();
    mock_parameter_client_ = make_shared<NiceMock<MockParameterClient>>();
    EXPECT_SUCCESS(mock_instance_client_->Init());
    EXPECT_SUCCESS(mock_instance_client_->Run());
    EXPECT_SUCCESS(mock_parameter_client_->Init());
    EXPECT_SUCCESS(mock_parameter_client_->Run());
    fetcher_ = make_unique<MockConfigurationFetcherWithOverrides>(
        mock_instance_client_, mock_parameter_client_, std::nullopt,
        std::nullopt);

    EXPECT_SUCCESS(fetcher_->Init());
    ExpectGetCurrentInstanceResourceNameSync(SuccessExecutionResult());
    ExpectGetInstanceDetailsSync(SuccessExecutionResult(), env_name_label_);
    EXPECT_SUCCESS(fetcher_->Run());
  }

  void TearDown() override {
    EXPECT_SUCCESS(fetcher_->Stop());
    EXPECT_SUCCESS(mock_parameter_client_->Stop());
    EXPECT_SUCCESS(mock_instance_client_->Stop());
  }

  void ExpectGetCurrentInstanceResourceNameSync(const ExecutionResult& result) {
    EXPECT_CALL(*mock_instance_client_, GetCurrentInstanceResourceNameSync)
        .WillOnce([result](GetCurrentInstanceResourceNameRequest request)
                      -> core::ExecutionResultOr<
                          GetCurrentInstanceResourceNameResponse> {
          GetCurrentInstanceResourceNameResponse response;
          if (result.Successful()) {
            response.set_instance_resource_name(kInstanceResourceName);
            return response;
          }
          return result;
        });
  }

  void ExpectGetInstanceDetailsSync(const ExecutionResult& result,
                                    const string& label) {
    EXPECT_CALL(*mock_instance_client_, GetInstanceDetailsByResourceNameSync)
        .WillOnce([result, label](  // Avoid capturing reference of reference.
                      GetInstanceDetailsByResourceNameRequest request)
                      -> core::ExecutionResultOr<
                          GetInstanceDetailsByResourceNameResponse> {
          GetInstanceDetailsByResourceNameResponse response;
          if (result.Successful() &&
              request.instance_resource_name() == kInstanceResourceName) {
            response.mutable_instance_details()->mutable_labels()->insert(
                MapPair<string, string>(label, string(kEnvName)));
            return response;
          }
          return result;
        });
  }

  void ExpectGetParameter(const ExecutionResult& result,
                          const string& parameter_name,
                          const string& parameter_value,
                          const string& parameter_name_prefix = "scp-",
                          const string& env_name = string(kEnvName)) {
    EXPECT_CALL(*mock_parameter_client_, GetParameter)
        .WillOnce(
            [result, parameter_name, parameter_value, parameter_name_prefix,
             env_name](AsyncContext<GetParameterRequest, GetParameterResponse>&
                           context) {
              context.result = result;
              auto response = make_shared<GetParameterResponse>();
              string env_name_prefix =
                  env_name.empty() ? "" : absl::StrCat(kEnvName, "-");
              if (result.Successful() &&
                  context.request->parameter_name() ==
                      absl::StrCat(parameter_name_prefix, env_name_prefix,
                                   parameter_name)) {
                response->set_parameter_value(parameter_value);
                context.response = std::move(response);
              }
              context.Finish();
            });
  }

  shared_ptr<MockInstanceClient> mock_instance_client_;
  shared_ptr<MockParameterClient> mock_parameter_client_;
  unique_ptr<ConfigurationFetcher> fetcher_;
  string env_name_label_ = string(kEnvNameLabel);
};

TEST_F(ConfigurationFetcherTest, GetInstanceResourceNameSyncSucceeded) {
  EXPECT_THAT(
      fetcher_->GetCurrentInstanceResourceNameSync(GetConfigurationRequest()),
      IsSuccessfulAndHolds(kInstanceResourceName));
}

TEST_F(ConfigurationFetcherTest, GetEnvironmentNameSyncSucceeded) {
  EXPECT_THAT(fetcher_->GetEnvironmentNameSync(GetConfigurationRequest()),
              IsSuccessfulAndHolds(kEnvName));
}

TEST_F(ConfigurationFetcherTest,
       GetEnvironmentNameSyncSucceededWithDifferentEnvNameLabel) {
  ExpectGetCurrentInstanceResourceNameSync(SuccessExecutionResult());
  string env_name_label = "different_label";
  ExpectGetInstanceDetailsSync(SuccessExecutionResult(), env_name_label);
  auto fetcher = make_unique<MockConfigurationFetcherWithOverrides>(
      mock_instance_client_, mock_parameter_client_, std::nullopt,
      env_name_label);
  EXPECT_SUCCESS(fetcher->Init());
  EXPECT_SUCCESS(fetcher->Run());
  EXPECT_THAT(fetcher->GetEnvironmentNameSync(GetConfigurationRequest()),
              IsSuccessfulAndHolds(kEnvName));
  EXPECT_SUCCESS(fetcher->Stop());
}

TEST_F(ConfigurationFetcherTest, GetParameterByNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     kTestTable);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<string, string>(
      make_shared<string>(JobClientProto::ClientConfigurationKeys_Name(
          JobClientProto::ClientConfigurationKeys::
              CMRT_JOB_CLIENT_JOB_TABLE_NAME)),
      [&finished](AsyncContext<string, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestTable);
        finished = true;
      });
  fetcher_->GetParameterByName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetParameterByNameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     kTestTable);
  EXPECT_THAT(fetcher_->GetParameterByNameSync(
                  JobClientProto::ClientConfigurationKeys_Name(
                      JobClientProto::ClientConfigurationKeys::
                          CMRT_JOB_CLIENT_JOB_TABLE_NAME)),
              IsSuccessfulAndHolds(kTestTable));
}

TEST_F(ConfigurationFetcherTest,
       GetParameterByNameSyncSucceededWithDifferentParameterNamePrefix) {
  ExpectGetCurrentInstanceResourceNameSync(SuccessExecutionResult());
  string env_name_label = "different_label";
  string parameter_name_prefix = "different-name-prefix-";
  ExpectGetInstanceDetailsSync(SuccessExecutionResult(), env_name_label);
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     kTestTable, parameter_name_prefix);
  auto fetcher = make_unique<MockConfigurationFetcherWithOverrides>(
      mock_instance_client_, mock_parameter_client_, parameter_name_prefix,
      env_name_label);
  EXPECT_SUCCESS(fetcher->Init());
  EXPECT_SUCCESS(fetcher->Run());
  EXPECT_THAT(fetcher->GetParameterByNameSync(
                  JobClientProto::ClientConfigurationKeys_Name(
                      JobClientProto::ClientConfigurationKeys::
                          CMRT_JOB_CLIENT_JOB_TABLE_NAME)),
              IsSuccessfulAndHolds(kTestTable));
  EXPECT_SUCCESS(fetcher->Stop());
}

TEST_F(ConfigurationFetcherTest,
       GetParameterByNameSyncSucceededWithEmptyEnvNameLabel) {
  string parameter_name_prefix = "different-name-prefix-";
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     kTestTable, parameter_name_prefix, "");
  fetcher_ = make_unique<MockConfigurationFetcherWithOverrides>(
      mock_instance_client_, mock_parameter_client_, parameter_name_prefix, "");
  EXPECT_SUCCESS(fetcher_->Init());
  EXPECT_SUCCESS(fetcher_->Run());
  EXPECT_THAT(fetcher_->GetParameterByNameSync(
                  JobClientProto::ClientConfigurationKeys_Name(
                      JobClientProto::ClientConfigurationKeys::
                          CMRT_JOB_CLIENT_JOB_TABLE_NAME)),
              IsSuccessfulAndHolds(kTestTable));
  EXPECT_SUCCESS(fetcher_->Stop());
}

TEST_F(ConfigurationFetcherTest,
       GetParameterByNameSyncFailedDueToEmptyParameterName) {
  EXPECT_THAT(fetcher_->GetParameterByNameSync(""),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_INVALID_PARAMETER_NAME)));
}

TEST_F(ConfigurationFetcherTest, GetUInt64ByNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     "123");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<string, uint64_t>(
      make_shared<string>(JobClientProto::ClientConfigurationKeys_Name(
          JobClientProto::ClientConfigurationKeys::
              CMRT_JOB_CLIENT_JOB_TABLE_NAME)),
      [&finished](AsyncContext<string, uint64_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 123);
        finished = true;
      });
  fetcher_->GetUInt64ByName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetUInt64NameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     "123");
  EXPECT_THAT(fetcher_->GetUInt64ByNameSync(
                  JobClientProto::ClientConfigurationKeys_Name(
                      JobClientProto::ClientConfigurationKeys::
                          CMRT_JOB_CLIENT_JOB_TABLE_NAME)),
              IsSuccessfulAndHolds(123));
}

TEST_F(ConfigurationFetcherTest,
       GetUInt64ByNameSyncFailedDueToEmptyParameterName) {
  EXPECT_THAT(fetcher_->GetUInt64ByNameSync(""),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_INVALID_PARAMETER_NAME)));
}

TEST_F(ConfigurationFetcherTest, GetBoolByNameSucceeded) {
  auto bool_param_name = "bool_param";
  ExpectGetParameter(SuccessExecutionResult(), bool_param_name, "true");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<string, bool>(
      make_shared<string>(bool_param_name),
      [&finished](AsyncContext<string, bool> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_TRUE(*context.response);
        finished = true;
      });
  fetcher_->GetBoolByName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetBoolNameSyncSucceeded) {
  auto bool_param_name = "bool_param";
  ExpectGetParameter(SuccessExecutionResult(), bool_param_name, "true");
  EXPECT_THAT(fetcher_->GetBoolByNameSync(bool_param_name),
              IsSuccessfulAndHolds(true));
}

TEST_F(ConfigurationFetcherTest,
       GetBoolByNameSyncFailedDueToEmptyParameterName) {
  EXPECT_THAT(fetcher_->GetBoolByNameSync(""),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_INVALID_PARAMETER_NAME)));
}

TEST_F(ConfigurationFetcherTest, GetParameterListByNameSucceeded) {
  auto list_param_name = "list_param";
  ExpectGetParameter(SuccessExecutionResult(), list_param_name, "p1,p2,p3");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<string, vector<string>>(
      make_shared<string>(list_param_name),
      [&finished](AsyncContext<string, vector<string>> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, testing::ElementsAre("p1", "p2", "p3"));
        finished = true;
      });
  fetcher_->GetParameterListByName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetParameterListByNameSyncSucceeded) {
  auto list_param_name = "list_param";
  ExpectGetParameter(SuccessExecutionResult(), list_param_name, "p1,p2,p3");
  EXPECT_THAT(fetcher_->GetParameterListByNameSync(list_param_name),
              IsSuccessfulAndHolds(testing::ElementsAre("p1", "p2", "p3")));
}

TEST_F(ConfigurationFetcherTest,
       GetParameterListByNameSyncFailedDueToEmptyParameterName) {
  EXPECT_THAT(fetcher_->GetParameterListByNameSync(""),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_INVALID_PARAMETER_NAME)));
}

TEST_F(ConfigurationFetcherTest, GetCommonLogOptionSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     CommonClientConfigurationKeys_Name(
                         CommonClientConfigurationKeys::CMRT_COMMON_LOG_OPTION),
                     kTestLogOption);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, LogOption>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, LogOption> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, LogOption::kConsoleLog);
        finished = true;
      });
  fetcher_->GetCommonLogOption(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetCommonLogOptionSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     CommonClientConfigurationKeys_Name(
                         CommonClientConfigurationKeys::CMRT_COMMON_LOG_OPTION),
                     kTestLogOption);
  EXPECT_THAT(fetcher_->GetCommonLogOptionSync(GetConfigurationRequest()),
              IsSuccessfulAndHolds(LogOption::kConsoleLog));
}

TEST_F(ConfigurationFetcherTest, GetCommonEnabledLogLevelsSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_ENABLED_LOG_LEVELS),
      kTestLogLevels);
  atomic<bool> finished = false;
  auto get_context =
      AsyncContext<GetConfigurationRequest, unordered_set<LogLevel>>(
          nullptr,
          [&finished](
              AsyncContext<GetConfigurationRequest, unordered_set<LogLevel>>
                  context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_THAT(
                *context.response,
                UnorderedElementsAre(LogLevel::kInfo, LogLevel::kDebug));
            finished = true;
          });
  fetcher_->GetCommonEnabledLogLevels(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetCommonEnabledLogLevelsSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_ENABLED_LOG_LEVELS),
      kTestLogLevels);
  EXPECT_THAT(
      *fetcher_->GetCommonEnabledLogLevelsSync(GetConfigurationRequest()),
      UnorderedElementsAre(LogLevel::kInfo, LogLevel::kDebug));
}

TEST_F(ConfigurationFetcherTest, GetCommonCpuThreadCountSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_COUNT),
      kTestCommonThreadCount);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10);
        finished = true;
      });
  fetcher_->GetCommonCpuThreadCount(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetCommonCpuThreadCountSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_COUNT),
      kTestCommonThreadCount);
  EXPECT_THAT(fetcher_->GetCommonCpuThreadCountSync(GetConfigurationRequest()),
              IsSuccessfulAndHolds(10));
}

TEST_F(ConfigurationFetcherTest, GetCommonCpuThreadCountExceedingMin) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_COUNT),
      "-1");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
        finished = true;
      });
  fetcher_->GetCommonCpuThreadCount(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetCommonCpuThreadCountSyncExceedingMax) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_COUNT),
      "18446744073709551616");  // Exceeding uint64_t
  EXPECT_THAT(fetcher_->GetCommonCpuThreadCountSync(GetConfigurationRequest()),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
}

TEST_F(ConfigurationFetcherTest, GetCommonCpuThreadPoolQueueCapSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_POOL_QUEUE_CAP),
      kTestCommonThreadPoolQueueCap);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10000);
        finished = true;
      });
  fetcher_->GetCommonCpuThreadPoolQueueCap(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetCommonCpuThreadPoolQueueCapSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_POOL_QUEUE_CAP),
      kTestCommonThreadPoolQueueCap);
  EXPECT_THAT(
      fetcher_->GetCommonCpuThreadPoolQueueCapSync(GetConfigurationRequest()),
      IsSuccessfulAndHolds(10000));
}

TEST_F(ConfigurationFetcherTest, GetCommonIoThreadCountSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_IO_THREAD_COUNT),
      kTestCommonThreadCount);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10);
        finished = true;
      });
  fetcher_->GetCommonIoThreadCount(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetCommonIoThreadCountSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_IO_THREAD_COUNT),
      kTestCommonThreadCount);
  EXPECT_THAT(fetcher_->GetCommonIoThreadCountSync(GetConfigurationRequest()),
              IsSuccessfulAndHolds(10));
}

TEST_F(ConfigurationFetcherTest, GetCommonIoThreadPoolQueueCapSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_IO_THREAD_POOL_QUEUE_CAP),
      kTestCommonThreadPoolQueueCap);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10000);
        finished = true;
      });
  fetcher_->GetCommonIoThreadPoolQueueCap(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetCommonIoThreadPoolQueueCapSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_IO_THREAD_POOL_QUEUE_CAP),
      kTestCommonThreadPoolQueueCap);
  EXPECT_THAT(
      fetcher_->GetCommonIoThreadPoolQueueCapSync(GetConfigurationRequest()),
      IsSuccessfulAndHolds(10000));
}

TEST_F(ConfigurationFetcherTest, GetJobLifecycleHelperRetryLimitSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobLifecycleHelperProto::ClientConfigurationKeys_Name(
                         JobLifecycleHelperProto::ClientConfigurationKeys::
                             CMRT_JOB_LIFECYCLE_HELPER_RETRY_LIMIT),
                     kTestRetryLimit);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 3);
        finished = true;
      });
  fetcher_->GetJobLifecycleHelperRetryLimit(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetJobLifecycleHelperRetryLimitSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobLifecycleHelperProto::ClientConfigurationKeys_Name(
                         JobLifecycleHelperProto::ClientConfigurationKeys::
                             CMRT_JOB_LIFECYCLE_HELPER_RETRY_LIMIT),
                     kTestRetryLimit);
  EXPECT_THAT(
      fetcher_->GetJobLifecycleHelperRetryLimitSync(GetConfigurationRequest()),
      IsSuccessfulAndHolds(3));
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperVisibilityTimeoutExtendTimeSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_VISIBILITY_TIMEOUT_EXTEND_TIME),
      kTestVisibilityTimeoutExtendTime);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 30);
        finished = true;
      });
  fetcher_->GetJobLifecycleHelperVisibilityTimeoutExtendTime(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperVisibilityTimeoutExtendTimeSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_VISIBILITY_TIMEOUT_EXTEND_TIME),
      kTestVisibilityTimeoutExtendTime);
  EXPECT_THAT(fetcher_->GetJobLifecycleHelperVisibilityTimeoutExtendTimeSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(30));
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperJobProcessingTimeoutSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobLifecycleHelperProto::ClientConfigurationKeys_Name(
                         JobLifecycleHelperProto::ClientConfigurationKeys::
                             CMRT_JOB_LIFECYCLE_HELPER_JOB_PROCESSING_TIMEOUT),
                     kTestJobProcessingTimeout);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 120);
        finished = true;
      });
  fetcher_->GetJobLifecycleHelperJobProcessingTimeout(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperJobProcessingTimeoutSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobLifecycleHelperProto::ClientConfigurationKeys_Name(
                         JobLifecycleHelperProto::ClientConfigurationKeys::
                             CMRT_JOB_LIFECYCLE_HELPER_JOB_PROCESSING_TIMEOUT),
                     kTestJobProcessingTimeout);
  EXPECT_THAT(fetcher_->GetJobLifecycleHelperJobProcessingTimeoutSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(120));
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperJobExtendingWorkerSleepTimeSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_EXTENDING_WORKER_SLEEP_TIME),
      kTestJobExtendingWorkerSleepTime);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 60);
        finished = true;
      });
  fetcher_->GetJobLifecycleHelperJobExtendingWorkerSleepTime(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperJobExtendingWorkerSleepTimeSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_EXTENDING_WORKER_SLEEP_TIME),
      kTestJobExtendingWorkerSleepTime);
  EXPECT_THAT(fetcher_->GetJobLifecycleHelperJobExtendingWorkerSleepTimeSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(60));
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperEnableMetricRecordingSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_ENABLE_METRIC_RECORDING),
      "true");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, bool>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, bool> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_TRUE(*context.response);
        finished = true;
      });
  fetcher_->GetJobLifecycleHelperEnableMetricRecording(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperEnableMetricRecordingSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_ENABLE_METRIC_RECORDING),
      "false");
  EXPECT_THAT(fetcher_->GetJobLifecycleHelperEnableMetricRecordingSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(false));
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperMetricNamespaceSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobLifecycleHelperProto::ClientConfigurationKeys_Name(
                         JobLifecycleHelperProto::ClientConfigurationKeys::
                             CMRT_JOB_LIFECYCLE_HELPER_JOB_METRIC_NAMESPACE),
                     kTestMetricNamespace);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestMetricNamespace);
        finished = true;
      });
  fetcher_->GetJobLifecycleHelperMetricNamespace(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetJobLifecycleHelperMetricNamespaceSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobLifecycleHelperProto::ClientConfigurationKeys_Name(
                         JobLifecycleHelperProto::ClientConfigurationKeys::
                             CMRT_JOB_LIFECYCLE_HELPER_JOB_METRIC_NAMESPACE),
                     kTestMetricNamespace);
  EXPECT_THAT(fetcher_->GetJobLifecycleHelperMetricNamespaceSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestMetricNamespace));
}

TEST_F(ConfigurationFetcherTest, GetJobClientJobQueueNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_QUEUE_NAME),
                     kTestQueue);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestQueue);
        finished = true;
      });
  fetcher_->GetJobClientJobQueueName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetJobClientJobQueueNameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_QUEUE_NAME),
                     kTestQueue);
  EXPECT_THAT(fetcher_->GetJobClientJobQueueNameSync(GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestQueue));
}

TEST_F(ConfigurationFetcherTest, GetJobClientJobTableNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     kTestTable);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestTable);
        finished = true;
      });
  fetcher_->GetJobClientJobTableName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetJobClientJobTableNameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     kTestTable);
  EXPECT_THAT(fetcher_->GetJobClientJobTableNameSync(GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestTable));
}

TEST_F(ConfigurationFetcherTest, GetGcpJobClientSpannerInstanceNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_GCP_JOB_CLIENT_SPANNER_INSTANCE_NAME),
                     kTestGcpSpannerInstance);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestGcpSpannerInstance);
        finished = true;
      });
  fetcher_->GetGcpJobClientSpannerInstanceName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetGcpJobClientSpannerInstanceNameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_GCP_JOB_CLIENT_SPANNER_INSTANCE_NAME),
                     kTestGcpSpannerInstance);
  EXPECT_THAT(fetcher_->GetGcpJobClientSpannerInstanceNameSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestGcpSpannerInstance));
}

TEST_F(ConfigurationFetcherTest, GetGcpJobClientSpannerDatabaseNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_GCP_JOB_CLIENT_SPANNER_DATABASE_NAME),
                     kTestGcpSpannerDatabase);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestGcpSpannerDatabase);
        finished = true;
      });
  fetcher_->GetGcpJobClientSpannerDatabaseName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetGcpJobClientSpannerDatabaseNameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_GCP_JOB_CLIENT_SPANNER_DATABASE_NAME),
                     kTestGcpSpannerDatabase);
  EXPECT_THAT(fetcher_->GetGcpJobClientSpannerDatabaseNameSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestGcpSpannerDatabase));
}

TEST_F(ConfigurationFetcherTest, GetJobClientJobTableNameSyncFailed) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  ExpectGetParameter(failure,
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                     kTestTable);
  EXPECT_THAT(fetcher_->GetJobClientJobTableNameSync(GetConfigurationRequest())
                  .result(),
              ResultIs(failure));
}

TEST_F(ConfigurationFetcherTest, GetJobClientReadJobRetryIntervalSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_READ_JOB_RETRY_INTERVAL_IN_MS),
                     kTestRetryInterval);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 123456);
        finished = true;
      });
  fetcher_->GetJobClientReadJobRetryInterval(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetJobClientReadJobRetryIntervalSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_READ_JOB_RETRY_INTERVAL_IN_MS),
                     kTestRetryInterval);
  EXPECT_THAT(
      fetcher_->GetJobClientReadJobRetryIntervalSync(GetConfigurationRequest()),
      IsSuccessfulAndHolds(123456));
}

TEST_F(ConfigurationFetcherTest, GetJobClientReadJobMaxRetryCountSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_READ_JOB_MAX_RETRY_COUNT),
                     kTestRetryLimit);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 3);
        finished = true;
      });
  fetcher_->GetJobClientReadJobMaxRetryCount(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetJobClientReadJobMaxRetryCountSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     JobClientProto::ClientConfigurationKeys_Name(
                         JobClientProto::ClientConfigurationKeys::
                             CMRT_JOB_CLIENT_READ_JOB_MAX_RETRY_COUNT),
                     kTestRetryLimit);
  EXPECT_THAT(
      fetcher_->GetJobClientReadJobMaxRetryCountSync(GetConfigurationRequest()),
      IsSuccessfulAndHolds(3));
}

TEST_F(ConfigurationFetcherTest,
       GetGcpNoSQLDatabaseClientSpannerInstanceNameSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      NoSQLDatabaseClientProto::ClientConfigurationKeys_Name(
          NoSQLDatabaseClientProto::ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_INSTANCE_NAME),
      kTestGcpSpannerInstance);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestGcpSpannerInstance);
        finished = true;
      });
  fetcher_->GetGcpNoSQLDatabaseClientSpannerInstanceName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetGcpNoSQLDatabaseClientSpannerInstanceNameSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      NoSQLDatabaseClientProto::ClientConfigurationKeys_Name(
          NoSQLDatabaseClientProto::ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_INSTANCE_NAME),
      kTestGcpSpannerInstance);
  EXPECT_THAT(fetcher_->GetGcpNoSQLDatabaseClientSpannerInstanceNameSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestGcpSpannerInstance));
}

TEST_F(ConfigurationFetcherTest,
       GetGcpNoSQLDatabaseClientSpannerDatabaseNameSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      NoSQLDatabaseClientProto::ClientConfigurationKeys_Name(
          NoSQLDatabaseClientProto::ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_DATABASE_NAME),
      kTestGcpSpannerDatabase);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestGcpSpannerDatabase);
        finished = true;
      });
  fetcher_->GetGcpNoSQLDatabaseClientSpannerDatabaseName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetGcpNoSQLDatabaseClientSpannerDatabaseNameSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      NoSQLDatabaseClientProto::ClientConfigurationKeys_Name(
          NoSQLDatabaseClientProto::ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_DATABASE_NAME),
      kTestGcpSpannerDatabase);
  EXPECT_THAT(fetcher_->GetGcpNoSQLDatabaseClientSpannerDatabaseNameSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestGcpSpannerDatabase));
}

TEST_F(ConfigurationFetcherTest, GetQueueClientQueueNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     QueueClientProto::ClientConfigurationKeys_Name(
                         QueueClientProto::ClientConfigurationKeys::
                             CMRT_QUEUE_CLIENT_QUEUE_NAME),
                     kTestQueue);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestQueue);
        finished = true;
      });
  fetcher_->GetQueueClientQueueName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest, GetQueueClientQueueNameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     QueueClientProto::ClientConfigurationKeys_Name(
                         QueueClientProto::ClientConfigurationKeys::
                             CMRT_QUEUE_CLIENT_QUEUE_NAME),
                     kTestQueue);
  EXPECT_THAT(fetcher_->GetQueueClientQueueNameSync(GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestQueue));
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientEnableRemoteMetricAggregationSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_ENABLE_REMOTE_METRIC_AGGREGATION),
      "true");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, bool>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, bool> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_TRUE(*context.response);
        finished = true;
      });
  fetcher_->GetMetricClientEnableRemoteMetricAggregation(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientEnableRemoteMetricAggregationSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_ENABLE_REMOTE_METRIC_AGGREGATION),
      "false");
  EXPECT_THAT(fetcher_->GetMetricClientEnableRemoteMetricAggregationSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(false));
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientEnableNativeAggregationSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_ENABLE_NATIVE_METRIC_AGGREGATION),
      "true");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, bool>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, bool> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_TRUE(*context.response);
        finished = true;
      });
  fetcher_->GetMetricClientEnableNativeMetricAggregation(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientEnableNativeMetricAggregationSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_ENABLE_NATIVE_METRIC_AGGREGATION),
      "false");
  EXPECT_THAT(fetcher_->GetMetricClientEnableNativeMetricAggregationSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(false));
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientRemoteMetricCollectorAddressSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_REMOTE_METRIC_COLLECTOR_ADDRESS),
      kTestMetricOpenTelemetryCollectorAddress);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestMetricOpenTelemetryCollectorAddress);
        finished = true;
      });
  fetcher_->GetMetricClientRemoteMetricCollectorAddress(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientRemoteMetricCollectorAddressSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_REMOTE_METRIC_COLLECTOR_ADDRESS),
      kTestMetricOpenTelemetryCollectorAddress);
  EXPECT_THAT(fetcher_->GetMetricClientRemoteMetricCollectorAddressSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestMetricOpenTelemetryCollectorAddress));
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientMetricExporterIntervalInMsSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     MetricClientProto::ClientConfigurationKeys_Name(
                         MetricClientProto::ClientConfigurationKeys::
                             CMRT_METRIC_CLIENT_METRIC_EXPORTER_INTERVAL_IN_MS),
                     "10000");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10000);
        finished = true;
      });
  fetcher_->GetMetricClientMetricExporterIntervalInMs(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientMetricExporterIntervalInMsSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     MetricClientProto::ClientConfigurationKeys_Name(
                         MetricClientProto::ClientConfigurationKeys::
                             CMRT_METRIC_CLIENT_METRIC_EXPORTER_INTERVAL_IN_MS),
                     "10000");
  EXPECT_THAT(fetcher_->GetMetricClientMetricExporterIntervalInMsSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(10000));
}

TEST_F(ConfigurationFetcherTest, GetMetricClientEnableBatchRecordingSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     MetricClientProto::ClientConfigurationKeys_Name(
                         MetricClientProto::ClientConfigurationKeys::
                             CMRT_METRIC_CLIENT_ENABLE_BATCH_RECORDING),
                     "true");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, bool>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, bool> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_TRUE(*context.response);
        finished = true;
      });
  fetcher_->GetMetricClientEnableBatchRecording(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientEnableBatchRecordingSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     MetricClientProto::ClientConfigurationKeys_Name(
                         MetricClientProto::ClientConfigurationKeys::
                             CMRT_METRIC_CLIENT_ENABLE_BATCH_RECORDING),
                     "false");
  EXPECT_THAT(fetcher_->GetMetricClientEnableBatchRecordingSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(false));
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientNamespaceForBatchRecordingSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     MetricClientProto::ClientConfigurationKeys_Name(
                         MetricClientProto::ClientConfigurationKeys::
                             CMRT_METRIC_CLIENT_NAMESPACE_FOR_BATCH_RECORDING),
                     kTestMetricNamespace);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestMetricNamespace);
        finished = true;
      });
  fetcher_->GetMetricClientNamespaceForBatchRecording(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientNamespaceForBatchRecordingSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     MetricClientProto::ClientConfigurationKeys_Name(
                         MetricClientProto::ClientConfigurationKeys::
                             CMRT_METRIC_CLIENT_NAMESPACE_FOR_BATCH_RECORDING),
                     kTestMetricNamespace);
  EXPECT_THAT(fetcher_->GetMetricClientNamespaceForBatchRecordingSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestMetricNamespace));
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientBatchRecordingTimeDurationInMsSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_BATCH_RECORDING_TIME_DURATION_IN_MS),
      "1000000");
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 1000000);
        finished = true;
      });
  fetcher_->GetMetricClientBatchRecordingTimeDurationInMs(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetMetricClientBatchRecordingTimeDurationInMsSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_BATCH_RECORDING_TIME_DURATION_IN_MS),
      "1000000");
  EXPECT_THAT(fetcher_->GetMetricClientBatchRecordingTimeDurationInMsSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(1000000));
}

TEST_F(ConfigurationFetcherTest,
       GetAutoScalingClientInstanceTableNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     AutoScalingClientProto::ClientConfigurationKeys_Name(
                         AutoScalingClientProto::ClientConfigurationKeys::
                             CMRT_AUTO_SCALING_CLIENT_INSTANCE_TABLE_NAME),
                     kTestInstanceTableName);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestInstanceTableName);
        finished = true;
      });
  fetcher_->GetAutoScalingClientInstanceTableName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetAutoScalingClientInstanceTableNameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     AutoScalingClientProto::ClientConfigurationKeys_Name(
                         AutoScalingClientProto::ClientConfigurationKeys::
                             CMRT_AUTO_SCALING_CLIENT_INSTANCE_TABLE_NAME),
                     kTestInstanceTableName);
  EXPECT_THAT(fetcher_->GetAutoScalingClientInstanceTableNameSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestInstanceTableName));
}

TEST_F(ConfigurationFetcherTest,
       GetAutoScalingClientSpannerInstanceNameSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_INSTANCE_NAME),
      kTestGcpSpannerInstance);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestGcpSpannerInstance);
        finished = true;
      });
  fetcher_->GetAutoScalingClientSpannerInstanceName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetAutoScalingClientSpannerInstanceNameSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_INSTANCE_NAME),
      kTestGcpSpannerInstance);
  EXPECT_THAT(fetcher_->GetAutoScalingClientSpannerInstanceNameSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestGcpSpannerInstance));
}

TEST_F(ConfigurationFetcherTest,
       GetAutoScalingClientSpannerDatabaseNameSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_DATABASE_NAME),
      kTestGcpSpannerDatabase);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestGcpSpannerDatabase);
        finished = true;
      });
  fetcher_->GetAutoScalingClientSpannerDatabaseName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetAutoScalingClientSpannerDatabaseNameSyncSucceeded) {
  ExpectGetParameter(
      SuccessExecutionResult(),
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_DATABASE_NAME),
      kTestGcpSpannerDatabase);
  EXPECT_THAT(fetcher_->GetAutoScalingClientSpannerDatabaseNameSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestGcpSpannerDatabase));
}

TEST_F(ConfigurationFetcherTest, GetAutoScalingClientScaleInHookNameSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     AutoScalingClientProto::RequestConfigurationKeys_Name(
                         AutoScalingClientProto::RequestConfigurationKeys::
                             CMRT_AUTO_SCALING_CLIENT_SCALE_IN_HOOK_NAME),
                     kTestScaleInHookName);
  atomic<bool> finished = false;
  auto get_context = AsyncContext<GetConfigurationRequest, string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestScaleInHookName);
        finished = true;
      });
  fetcher_->GetAutoScalingClientScaleInHookName(get_context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(ConfigurationFetcherTest,
       GetAutoScalingClientScaleInHookNameSyncSucceeded) {
  ExpectGetParameter(SuccessExecutionResult(),
                     AutoScalingClientProto::RequestConfigurationKeys_Name(
                         AutoScalingClientProto::RequestConfigurationKeys::
                             CMRT_AUTO_SCALING_CLIENT_SCALE_IN_HOOK_NAME),
                     kTestScaleInHookName);
  EXPECT_THAT(fetcher_->GetAutoScalingClientScaleInHookNameSync(
                  GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestScaleInHookName));
}

TEST_F(ConfigurationFetcherTest, FailedToGetInstanceResourceName) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  ExpectGetCurrentInstanceResourceNameSync(failure);

  auto fetcher = make_unique<MockConfigurationFetcherWithOverrides>(
      mock_instance_client_, mock_parameter_client_, std::nullopt,
      std::nullopt);
  EXPECT_SUCCESS(fetcher->Init());
  EXPECT_THAT(fetcher->Run(), ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  EXPECT_THAT(
      fetcher->GetCurrentInstanceResourceNameSync(GetConfigurationRequest()),
      ResultIs(FailureExecutionResult(
          SC_CONFIGURATION_FETCHER_INSTANCE_RESOURCE_NAME_NOT_FOUND)));
}

TEST_F(ConfigurationFetcherTest, EnvNameNotFound) {
  ExpectGetCurrentInstanceResourceNameSync(SuccessExecutionResult());
  ExpectGetInstanceDetailsSync(SuccessExecutionResult(), "invalid_label");

  auto fetcher = make_unique<MockConfigurationFetcherWithOverrides>(
      mock_instance_client_, mock_parameter_client_, std::nullopt,
      std::nullopt);
  EXPECT_SUCCESS(fetcher->Init());
  EXPECT_THAT(fetcher->Run(),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND)));

  EXPECT_THAT(fetcher->GetEnvironmentNameSync(GetConfigurationRequest()),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND)));
}
}  // namespace google::scp::cpio
