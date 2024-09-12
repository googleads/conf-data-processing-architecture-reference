/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "configuration_fetcher.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "absl/strings/str_cat.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "core/async_executor/src/async_executor.h"
#include "core/curl_client/src/http1_curl_client.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/configuration_keys.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"
#include "public/cpio/proto/auto_scaling_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/common/v1/common_configuration_keys.pb.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"
#include "public/cpio/proto/job_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/metric_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/nosql_database_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "public/cpio/proto/queue_service/v1/configuration_keys.pb.h"
#include "public/cpio/utils/job_lifecycle_helper/proto/v1/configuration_keys.pb.h"
#include "public/cpio/utils/job_lifecycle_helper/proto/v1/job_lifecycle_helper.pb.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

#include "configuration_fetcher_utils.h"
#include "error_codes.h"

#if defined(AWS_CLIENT)
#include "cpio/client_providers/auth_token_provider/src/aws/aws_auth_token_provider.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_provider.h"
#include "public/cpio/utils/configuration_fetcher/src/aws/aws_instance_client.h"
#include "public/cpio/utils/configuration_fetcher/src/aws/aws_parameter_client.h"
#elif defined(GCP_CLIENT)
#include "core/http2_client/src/http2_client.h"
#include "cpio/client_providers/auth_token_provider/src/gcp/gcp_auth_token_provider.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_provider.h"
#include "public/cpio/utils/configuration_fetcher/src/gcp/gcp_instance_client.h"
#include "public/cpio/utils/configuration_fetcher/src/gcp/gcp_parameter_client.h"
#endif

namespace NoSQLDatabaseClientProto =
    google::cmrt::sdk::nosql_database_service::v1;
namespace AutoScalingClientProto = google::cmrt::sdk::auto_scaling_service::v1;
namespace QueueClientProto = google::cmrt::sdk::queue_service::v1;
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
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Http1CurlClient;
using google::scp::core::LogLevel;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_INVALID_ENVIRONMENT_NAME_LABEL;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_INVALID_PARAMETER_NAME;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::unordered_set;
using std::placeholders::_1;
using std::placeholders::_2;

namespace {
constexpr char kConfigurationFetcher[] = "ConfigurationFetcher";

constexpr int kDefaultCpuThreadCount = 2;
constexpr int kDefaultCpuThreadPoolQueueCap = 1000;
constexpr int kDefaultIoThreadCount = 2;
constexpr int kDefaultIoThreadPoolQueueCap = 1000;
}  // namespace

namespace google::scp::cpio {
void ConfigurationFetcher::CreateInstanceAndParameterClient() noexcept {
  cpu_async_executor_ = make_shared<AsyncExecutor>(
      kDefaultCpuThreadCount, kDefaultCpuThreadPoolQueueCap);
  io_async_executor_ = make_shared<AsyncExecutor>(kDefaultIoThreadCount,
                                                  kDefaultIoThreadPoolQueueCap);
  http1_client_ =
      make_shared<Http1CurlClient>(cpu_async_executor_, io_async_executor_);

#if defined(AWS_CLIENT)
  SCP_INFO(kConfigurationFetcher, kZeroUuid, "Start AWS Configuration Fetcher");
  auth_token_provider_ =
      make_shared<client_providers::AwsAuthTokenProvider>(http1_client_);
  instance_client_provider_ =
      make_shared<client_providers::AwsInstanceClientProvider>(
          auth_token_provider_, http1_client_, cpu_async_executor_,
          io_async_executor_);
  instance_client_ = make_shared<AwsInstanceClient>(
      make_shared<InstanceClientOptions>(), instance_client_provider_);
  parameter_client_ = make_shared<AwsParameterClient>(
      make_shared<ParameterClientOptions>(), instance_client_provider_,
      io_async_executor_);
#elif defined(GCP_CLIENT)
  SCP_INFO(kConfigurationFetcher, kZeroUuid, "Start GCP Configuration Fetcher");
  http2_client_ = make_shared<core::HttpClient>(cpu_async_executor_);

  auth_token_provider_ = make_shared<client_providers::GcpAuthTokenProvider>(
      http1_client_, io_async_executor_);

  instance_client_provider_ =
      make_shared<client_providers::GcpInstanceClientProvider>(
          auth_token_provider_, http1_client_, http2_client_);
  instance_client_ = make_shared<GcpInstanceClient>(
      make_shared<InstanceClientOptions>(), instance_client_provider_);
  parameter_client_ = make_shared<GcpParameterClient>(
      make_shared<ParameterClientOptions>(), instance_client_provider_,
      cpu_async_executor_, io_async_executor_);
#endif
}

ExecutionResult ConfigurationFetcher::Init() noexcept {
  CreateInstanceAndParameterClient();
  RETURN_IF_FAILURE(
      ConvertToPublicExecutionResult(cpu_async_executor_->Init()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(io_async_executor_->Init()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(http1_client_->Init()));
  if (http2_client_)
    RETURN_IF_FAILURE(ConvertToPublicExecutionResult(http2_client_->Init()));
  RETURN_IF_FAILURE(
      ConvertToPublicExecutionResult(auth_token_provider_->Init()));
  RETURN_IF_FAILURE(
      ConvertToPublicExecutionResult(instance_client_provider_->Init()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(instance_client_->Init()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(parameter_client_->Init()));
  return SuccessExecutionResult();
}

ExecutionResult ConfigurationFetcher::Run() noexcept {
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(cpu_async_executor_->Run()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(io_async_executor_->Run()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(http1_client_->Run()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(http2_client_->Run()));
  RETURN_IF_FAILURE(
      ConvertToPublicExecutionResult(auth_token_provider_->Run()));
  RETURN_IF_FAILURE(
      ConvertToPublicExecutionResult(instance_client_provider_->Run()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(instance_client_->Run()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(parameter_client_->Run()));
  return SuccessExecutionResult();
}

ExecutionResult ConfigurationFetcher::Stop() noexcept {
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(parameter_client_->Stop()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(instance_client_->Stop()));
  RETURN_IF_FAILURE(
      ConvertToPublicExecutionResult(instance_client_provider_->Stop()));
  RETURN_IF_FAILURE(
      ConvertToPublicExecutionResult(auth_token_provider_->Stop()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(http2_client_->Stop()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(http1_client_->Stop()));
  RETURN_IF_FAILURE(ConvertToPublicExecutionResult(io_async_executor_->Stop()));
  RETURN_IF_FAILURE(
      ConvertToPublicExecutionResult(cpu_async_executor_->Stop()));
  return SuccessExecutionResult();
}

ExecutionResultOr<string>
ConfigurationFetcher::GetCurrentInstanceResourceNameSync(
    GetConfigurationRequest request) noexcept {
  string instance_resource_name;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetCurrentInstanceResourceName, this, _1),
          request, instance_resource_name);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to get current instance resource name.");
  return instance_resource_name;
}

void ConfigurationFetcher::GetCurrentInstanceResourceName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto get_current_instance_resource_name_context =
      AsyncContext<GetCurrentInstanceResourceNameRequest,
                   GetCurrentInstanceResourceNameResponse>(
          make_shared<GetCurrentInstanceResourceNameRequest>(),
          bind(&ConfigurationFetcher::GetCurrentInstanceResourceNameCallback,
               this, _1, context),
          context);
  instance_client_->GetCurrentInstanceResourceName(
      get_current_instance_resource_name_context);
}

void ConfigurationFetcher::GetCurrentInstanceResourceNameCallback(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_current_instance_resource_name_context,
    AsyncContext<GetConfigurationRequest, string>&
        get_configuration_context) noexcept {
  get_configuration_context.result =
      get_current_instance_resource_name_context.result;
  if (!get_configuration_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kConfigurationFetcher, get_configuration_context,
                      get_configuration_context.result,
                      "Failed to GetCurrentInstanceResourceName");
    get_configuration_context.Finish();
    return;
  }
  get_configuration_context.response =
      make_shared<string>(get_current_instance_resource_name_context.response
                              ->instance_resource_name());
  get_configuration_context.Finish();
}

ExecutionResultOr<std::string> ConfigurationFetcher::GetEnvironmentNameSync(
    GetConfigurationRequest request) noexcept {
  string env_name;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetEnvironmentName, this, _1), request,
          env_name);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to get environment name.");
  return env_name;
}

void ConfigurationFetcher::GetEnvironmentName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  if (environment_name_label_.empty()) {
    context.result = FailureExecutionResult(
        SC_CONFIGURATION_FETCHER_INVALID_ENVIRONMENT_NAME_LABEL);
    SCP_ERROR_CONTEXT(kConfigurationFetcher, context, context.result,
                      "Environment name label is empty.");
    context.Finish();
    return;
  }

  auto get_current_instance_resource_name_context =
      AsyncContext<GetConfigurationRequest, string>(
          make_shared<GetConfigurationRequest>(),
          bind(&ConfigurationFetcher::
                   GetCurrentInstanceResourceNameForEnvNameCallback,
               this, _1, context),
          context);
  GetCurrentInstanceResourceName(get_current_instance_resource_name_context);
}

void ConfigurationFetcher::GetCurrentInstanceResourceNameForEnvNameCallback(
    AsyncContext<GetConfigurationRequest, string>&
        get_current_instance_resource_name_context,
    AsyncContext<GetConfigurationRequest, string>&
        get_env_name_context) noexcept {
  get_env_name_context.result =
      get_current_instance_resource_name_context.result;
  if (!get_env_name_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kConfigurationFetcher, get_env_name_context,
                      get_env_name_context.result,
                      "Failed to GetCurrentInstanceResourceName");
    get_env_name_context.Finish();
    return;
  }

  auto request = make_shared<GetInstanceDetailsByResourceNameRequest>();
  request->set_instance_resource_name(
      *get_current_instance_resource_name_context.response);
  auto get_instance_details_context =
      AsyncContext<GetInstanceDetailsByResourceNameRequest,
                   GetInstanceDetailsByResourceNameResponse>(
          move(request),
          bind(&ConfigurationFetcher::GetInstanceDetailsByResourceNameCallback,
               this, _1, get_env_name_context),
          get_env_name_context);
  instance_client_->GetInstanceDetailsByResourceName(
      get_instance_details_context);
}

void ConfigurationFetcher::GetInstanceDetailsByResourceNameCallback(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context,
    AsyncContext<GetConfigurationRequest, string>&
        get_env_name_context) noexcept {
  if (!get_instance_details_context.result.Successful()) {
    get_env_name_context.result = get_instance_details_context.result;
    SCP_ERROR_CONTEXT(
        kConfigurationFetcher, get_env_name_context,
        get_env_name_context.result,
        "Failed to GetInstanceDetailsByResourceName for instance %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_env_name_context.Finish();
    return;
  }

  auto it =
      get_instance_details_context.response->instance_details().labels().find(
          environment_name_label_);
  if (it == get_instance_details_context.response->instance_details()
                .labels()
                .end()) {
    get_env_name_context.result = FailureExecutionResult(
        SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kConfigurationFetcher, get_env_name_context,
        get_env_name_context.result,
        "Failed to find environment name for instance %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_env_name_context.Finish();
    return;
  }

  get_env_name_context.response = make_shared<string>(it->second);
  get_env_name_context.Finish();
}

ExecutionResultOr<string> ConfigurationFetcher::GetParameterByNameSync(
    string parameter_name) noexcept {
  string parameter;
  auto execution_result = SyncUtils::AsyncToSync2<string, string>(
      bind(&ConfigurationFetcher::GetParameterByName, this, _1), parameter_name,
      parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetParameterByName for %s.",
                            parameter_name.c_str());
  return parameter;
}

void ConfigurationFetcher::GetParameterByName(
    AsyncContext<string, string> context) noexcept {
  if (context.request->empty()) {
    context.result =
        FailureExecutionResult(SC_CONFIGURATION_FETCHER_INVALID_PARAMETER_NAME);
    SCP_ERROR_CONTEXT(kConfigurationFetcher, context, context.result,
                      "Parameter name is empty.");
    context.Finish();
    return;
  }

  GetConfiguration(context);
}

ExecutionResultOr<uint64_t> ConfigurationFetcher::GetUInt64ByNameSync(
    string parameter_name) noexcept {
  uint64_t parameter;
  auto execution_result = SyncUtils::AsyncToSync2<string, uint64_t>(
      bind(&ConfigurationFetcher::GetUInt64ByName, this, _1), parameter_name,
      parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetParameterByName for %s.",
                            parameter_name.c_str());
  return parameter;
}

void ConfigurationFetcher::GetUInt64ByName(
    AsyncContext<string, uint64_t> context) noexcept {
  if (context.request->empty()) {
    context.result =
        FailureExecutionResult(SC_CONFIGURATION_FETCHER_INVALID_PARAMETER_NAME);
    SCP_ERROR_CONTEXT(kConfigurationFetcher, context, context.result,
                      "Parameter name is empty.");
    context.Finish();
    return;
  }
  AsyncContext<std::string, std::string> string_context(
      context.request,
      [context](
          core::AsyncContext<std::string, std::string> string_context) mutable {
        context.result = string_context.result;
        if (context.result.Successful()) {
          auto convert_result =
              ConfigurationFetcherUtils::StringToUInt<uint64_t>(
                  *string_context.response);
          if (convert_result.Successful()) {
            context.response =
                std::make_shared<uint64_t>(convert_result.release());
          } else {
            context.result = convert_result.result();
          }
        }
        context.Finish();
      });

  GetConfiguration(string_context);
}

ExecutionResultOr<LogOption> ConfigurationFetcher::GetCommonLogOptionSync(
    GetConfigurationRequest request) noexcept {
  LogOption parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, LogOption>(
          bind(&ConfigurationFetcher::GetCommonLogOption, this, _1), request,
          parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetCommonLogOption %s.",
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_LOG_OPTION)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetCommonLogOption(
    AsyncContext<GetConfigurationRequest, LogOption> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<LogOption>(
          CommonClientConfigurationKeys_Name(
              CommonClientConfigurationKeys::CMRT_COMMON_LOG_OPTION),
          context,
          bind(ConfigurationFetcherUtils::StringToEnum<LogOption>, _1,
               kLogOptionConfigMap));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<unordered_set<LogLevel>>
ConfigurationFetcher::GetCommonEnabledLogLevelsSync(
    GetConfigurationRequest request) noexcept {
  unordered_set<LogLevel> parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, unordered_set<LogLevel>>(
          bind(&ConfigurationFetcher::GetCommonEnabledLogLevels, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetCommonEnabledLogLevels %s.",
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_ENABLED_LOG_LEVELS)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetCommonEnabledLogLevels(
    AsyncContext<GetConfigurationRequest, unordered_set<LogLevel>>
        context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<
          unordered_set<LogLevel>>(
          CommonClientConfigurationKeys_Name(
              CommonClientConfigurationKeys::CMRT_COMMON_ENABLED_LOG_LEVELS),
          context,
          bind(ConfigurationFetcherUtils::StringToEnumSet<LogLevel>, _1,
               kLogLevelConfigMap));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t> ConfigurationFetcher::GetCommonCpuThreadCountSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetCommonCpuThreadCount, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetCommonCpuThreadCount %s.",
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_COUNT)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetCommonCpuThreadCount(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          CommonClientConfigurationKeys_Name(
              CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_COUNT),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetCommonCpuThreadPoolQueueCapSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetCommonCpuThreadPoolQueueCap, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetCommonCpuThreadPoolQueueCap %s.",
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_CPU_THREAD_POOL_QUEUE_CAP)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetCommonCpuThreadPoolQueueCap(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          CommonClientConfigurationKeys_Name(
              CommonClientConfigurationKeys::
                  CMRT_COMMON_CPU_THREAD_POOL_QUEUE_CAP),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t> ConfigurationFetcher::GetCommonIoThreadCountSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetCommonIoThreadCount, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetCommonIoThreadCount %s.",
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_IO_THREAD_COUNT)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetCommonIoThreadCount(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          CommonClientConfigurationKeys_Name(
              CommonClientConfigurationKeys::CMRT_COMMON_IO_THREAD_COUNT),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetCommonIoThreadPoolQueueCapSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetCommonIoThreadPoolQueueCap, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetCommonIoThreadPoolQueueCap %s.",
      CommonClientConfigurationKeys_Name(
          CommonClientConfigurationKeys::CMRT_COMMON_IO_THREAD_POOL_QUEUE_CAP)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetCommonIoThreadPoolQueueCap(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          CommonClientConfigurationKeys_Name(
              CommonClientConfigurationKeys::
                  CMRT_COMMON_IO_THREAD_POOL_QUEUE_CAP),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetJobLifecycleHelperRetryLimitSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetJobLifecycleHelperRetryLimit, this,
               _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetJobLifecycleHelperRetryLimit %s.",
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_RETRY_LIMIT)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobLifecycleHelperRetryLimit(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          JobLifecycleHelperProto::ClientConfigurationKeys_Name(
              JobLifecycleHelperProto::ClientConfigurationKeys::
                  CMRT_JOB_LIFECYCLE_HELPER_RETRY_LIMIT),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetJobLifecycleHelperVisibilityTimeoutExtendTimeSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::
                   GetJobLifecycleHelperVisibilityTimeoutExtendTime,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetJobLifecycleHelperVisibilityTimeoutExtendTime %s.",
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_VISIBILITY_TIMEOUT_EXTEND_TIME)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobLifecycleHelperVisibilityTimeoutExtendTime(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          JobLifecycleHelperProto::ClientConfigurationKeys_Name(
              JobLifecycleHelperProto::ClientConfigurationKeys::
                  CMRT_JOB_LIFECYCLE_HELPER_VISIBILITY_TIMEOUT_EXTEND_TIME),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetJobLifecycleHelperJobProcessingTimeoutSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetJobLifecycleHelperJobProcessingTimeout,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetJobLifecycleHelperJobProcessingTimeout %s.",
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_PROCESSING_TIMEOUT)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobLifecycleHelperJobProcessingTimeout(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          JobLifecycleHelperProto::ClientConfigurationKeys_Name(
              JobLifecycleHelperProto::ClientConfigurationKeys::
                  CMRT_JOB_LIFECYCLE_HELPER_JOB_PROCESSING_TIMEOUT),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetJobLifecycleHelperJobExtendingWorkerSleepTimeSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::
                   GetJobLifecycleHelperJobExtendingWorkerSleepTime,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetJobLifecycleHelperJobExtendingWorkerSleepTime %s.",
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_EXTENDING_WORKER_SLEEP_TIME)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobLifecycleHelperJobExtendingWorkerSleepTime(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          JobLifecycleHelperProto::ClientConfigurationKeys_Name(
              JobLifecycleHelperProto::ClientConfigurationKeys::
                  CMRT_JOB_LIFECYCLE_HELPER_JOB_EXTENDING_WORKER_SLEEP_TIME),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<bool>
ConfigurationFetcher::GetJobLifecycleHelperEnableMetricRecordingSync(
    GetConfigurationRequest request) noexcept {
  bool result;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, bool>(
          bind(
              &ConfigurationFetcher::GetJobLifecycleHelperEnableMetricRecording,
              this, _1),
          request, result);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetJobLifecycleHelperEnableMetricRecording %s.",
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_ENABLE_METRIC_RECORDING)
          .c_str());
  return result;
}

void ConfigurationFetcher::GetJobLifecycleHelperEnableMetricRecording(
    AsyncContext<GetConfigurationRequest, bool> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<bool>(
          JobLifecycleHelperProto::ClientConfigurationKeys_Name(
              JobLifecycleHelperProto::ClientConfigurationKeys::
                  CMRT_JOB_LIFECYCLE_HELPER_JOB_ENABLE_METRIC_RECORDING),
          context, bind(ConfigurationFetcherUtils::StringToBool, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetJobLifecycleHelperMetricNamespaceSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetJobLifecycleHelperMetricNamespace,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetJobLifecycleHelperMetricNamespace %s.",
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_METRIC_NAMESPACE)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobLifecycleHelperMetricNamespace(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      JobLifecycleHelperProto::ClientConfigurationKeys_Name(
          JobLifecycleHelperProto::ClientConfigurationKeys::
              CMRT_JOB_LIFECYCLE_HELPER_JOB_METRIC_NAMESPACE),
      context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string> ConfigurationFetcher::GetJobClientJobQueueNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetJobClientJobQueueName, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetJobClientJobQueueName %s.",
                            JobClientProto::ClientConfigurationKeys_Name(
                                JobClientProto::ClientConfigurationKeys::
                                    CMRT_JOB_CLIENT_JOB_QUEUE_NAME)
                                .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobClientJobQueueName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(JobClientProto::ClientConfigurationKeys_Name(
                                 JobClientProto::ClientConfigurationKeys::
                                     CMRT_JOB_CLIENT_JOB_QUEUE_NAME),
                             context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string> ConfigurationFetcher::GetJobClientJobTableNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetJobClientJobTableName, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetJobClientJobTableName %s.",
                            JobClientProto::ClientConfigurationKeys_Name(
                                JobClientProto::ClientConfigurationKeys::
                                    CMRT_JOB_CLIENT_JOB_TABLE_NAME)
                                .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobClientJobTableName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(JobClientProto::ClientConfigurationKeys_Name(
                                 JobClientProto::ClientConfigurationKeys::
                                     CMRT_JOB_CLIENT_JOB_TABLE_NAME),
                             context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetGcpJobClientSpannerInstanceNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetGcpJobClientSpannerInstanceName, this,
               _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,

                            "Failed to GetGcpJobClientSpannerInstanceName %s.",
                            JobClientProto::ClientConfigurationKeys_Name(
                                JobClientProto::ClientConfigurationKeys::
                                    CMRT_GCP_JOB_CLIENT_SPANNER_INSTANCE_NAME)
                                .c_str());
  return parameter;
}

void ConfigurationFetcher::GetGcpJobClientSpannerInstanceName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(JobClientProto::ClientConfigurationKeys_Name(
                                 JobClientProto::ClientConfigurationKeys::
                                     CMRT_GCP_JOB_CLIENT_SPANNER_INSTANCE_NAME),
                             context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetGcpJobClientSpannerDatabaseNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetGcpJobClientSpannerDatabaseName, this,
               _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,

                            "Failed to GetGcpJobClientSpannerDatabaseName %s.",
                            JobClientProto::ClientConfigurationKeys_Name(
                                JobClientProto::ClientConfigurationKeys::
                                    CMRT_GCP_JOB_CLIENT_SPANNER_DATABASE_NAME)
                                .c_str());
  return parameter;
}

void ConfigurationFetcher::GetGcpJobClientSpannerDatabaseName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(JobClientProto::ClientConfigurationKeys_Name(
                                 JobClientProto::ClientConfigurationKeys::
                                     CMRT_GCP_JOB_CLIENT_SPANNER_DATABASE_NAME),
                             context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetJobClientReadJobRetryIntervalSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetJobClientReadJobRetryInterval, this,
               _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetJobClientReadJobRetryInterval %s.",
      JobClientProto::ClientConfigurationKeys_Name(
          JobClientProto::ClientConfigurationKeys::
              CMRT_JOB_CLIENT_READ_JOB_RETRY_INTERVAL_IN_MS)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobClientReadJobRetryInterval(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          JobClientProto::ClientConfigurationKeys_Name(
              JobClientProto::ClientConfigurationKeys::
                  CMRT_JOB_CLIENT_READ_JOB_RETRY_INTERVAL_IN_MS),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetJobClientReadJobMaxRetryCountSync(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetJobClientReadJobMaxRetryCount, this,
               _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetJobClientReadJobMaxRetryCount %s.",
                            JobClientProto::ClientConfigurationKeys_Name(
                                JobClientProto::ClientConfigurationKeys::
                                    CMRT_JOB_CLIENT_READ_JOB_MAX_RETRY_COUNT)
                                .c_str());
  return parameter;
}

void ConfigurationFetcher::GetJobClientReadJobMaxRetryCount(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          JobClientProto::ClientConfigurationKeys_Name(
              JobClientProto::ClientConfigurationKeys::
                  CMRT_JOB_CLIENT_READ_JOB_MAX_RETRY_COUNT),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetGcpNoSQLDatabaseClientSpannerInstanceNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result = SyncUtils::AsyncToSync2<GetConfigurationRequest,
                                                  string>(
      bind(&ConfigurationFetcher::GetGcpNoSQLDatabaseClientSpannerInstanceName,
           this, _1),
      request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetGcpNoSQLDatabaseClientSpannerInstanceName %s.",
      NoSQLDatabaseClientProto::ClientConfigurationKeys_Name(
          NoSQLDatabaseClientProto::ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_INSTANCE_NAME)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetGcpNoSQLDatabaseClientSpannerInstanceName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      NoSQLDatabaseClientProto::ClientConfigurationKeys_Name(
          NoSQLDatabaseClientProto::ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_INSTANCE_NAME),
      context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetGcpNoSQLDatabaseClientSpannerDatabaseNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result = SyncUtils::AsyncToSync2<GetConfigurationRequest,
                                                  string>(
      bind(&ConfigurationFetcher::GetGcpNoSQLDatabaseClientSpannerDatabaseName,
           this, _1),
      request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetGcpNoSQLDatabaseClientSpannerDatabaseName %s.",
      NoSQLDatabaseClientProto::ClientConfigurationKeys_Name(
          NoSQLDatabaseClientProto::ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_DATABASE_NAME)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetGcpNoSQLDatabaseClientSpannerDatabaseName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      NoSQLDatabaseClientProto::ClientConfigurationKeys_Name(
          NoSQLDatabaseClientProto::ClientConfigurationKeys::
              CMRT_GCP_NOSQL_DATABASE_CLIENT_SPANNER_DATABASE_NAME),
      context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string> ConfigurationFetcher::GetQueueClientQueueNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetQueueClientQueueName, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetQueueClientQueueName %s.",
                            QueueClientProto::ClientConfigurationKeys_Name(
                                QueueClientProto::ClientConfigurationKeys::
                                    CMRT_QUEUE_CLIENT_QUEUE_NAME)
                                .c_str());
  return parameter;
}

void ConfigurationFetcher::GetQueueClientQueueName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(QueueClientProto::ClientConfigurationKeys_Name(
                                 QueueClientProto::ClientConfigurationKeys::
                                     CMRT_QUEUE_CLIENT_QUEUE_NAME),
                             context);
  GetConfiguration(context_with_parameter_name);
}

void ConfigurationFetcher::GetMetricClientEnableBatchRecording(
    AsyncContext<GetConfigurationRequest, bool> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<bool>(
          MetricClientProto::ClientConfigurationKeys_Name(
              MetricClientProto::ClientConfigurationKeys::
                  CMRT_METRIC_CLIENT_ENABLE_BATCH_RECORDING),
          context, bind(ConfigurationFetcherUtils::StringToBool, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<bool>
ConfigurationFetcher::GetMetricClientEnableBatchRecordingSync(
    GetConfigurationRequest request) noexcept {
  bool result;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, bool>(
          bind(&ConfigurationFetcher::GetMetricClientEnableBatchRecording, this,
               _1),
          request, result);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetMetricClientEnableBatchRecording %s.",
                            MetricClientProto::ClientConfigurationKeys_Name(
                                MetricClientProto::ClientConfigurationKeys::
                                    CMRT_METRIC_CLIENT_ENABLE_BATCH_RECORDING)
                                .c_str());
  return result;
}

void ConfigurationFetcher::GetMetricClientNamespaceForBatchRecording(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_NAMESPACE_FOR_BATCH_RECORDING),
      context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetMetricClientNamespaceForBatchRecordingSync(
    GetConfigurationRequest request) noexcept {
  string result;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetMetricClientNamespaceForBatchRecording,
               this, _1),
          request, result);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetMetricClientNamespaceForBatchRecording %s.",
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_NAMESPACE_FOR_BATCH_RECORDING)
          .c_str());
  return result;
}

void ConfigurationFetcher::GetMetricClientBatchRecordingTimeDurationInMs(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          MetricClientProto::ClientConfigurationKeys_Name(
              MetricClientProto::ClientConfigurationKeys::
                  CMRT_METRIC_CLIENT_BATCH_RECORDING_TIME_DURATION_IN_MS),
          context, bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t>
ConfigurationFetcher::GetMetricClientBatchRecordingTimeDurationInMsSync(
    GetConfigurationRequest request) noexcept {
  size_t result;
  auto execution_result = SyncUtils::AsyncToSync2<GetConfigurationRequest,
                                                  size_t>(
      bind(&ConfigurationFetcher::GetMetricClientBatchRecordingTimeDurationInMs,
           this, _1),
      request, result);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetMetricClientBatchRecordingTimeDurationInMs %s.",
      MetricClientProto::ClientConfigurationKeys_Name(
          MetricClientProto::ClientConfigurationKeys::
              CMRT_METRIC_CLIENT_BATCH_RECORDING_TIME_DURATION_IN_MS)
          .c_str());
  return result;
}

void ConfigurationFetcher::GetAutoScalingClientInstanceTableName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_AUTO_SCALING_CLIENT_INSTANCE_TABLE_NAME),
      context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetAutoScalingClientInstanceTableNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetAutoScalingClientInstanceTableName,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetAutoScalingClientInstanceTableName %s.",
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_AUTO_SCALING_CLIENT_INSTANCE_TABLE_NAME)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetAutoScalingClientSpannerInstanceName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_INSTANCE_NAME),
      context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetAutoScalingClientSpannerInstanceNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetAutoScalingClientSpannerInstanceName,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetAutoScalingClientSpannerInstanceName %s.",
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_INSTANCE_NAME)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetAutoScalingClientSpannerDatabaseName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_DATABASE_NAME),
      context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetAutoScalingClientSpannerDatabaseNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetAutoScalingClientSpannerDatabaseName,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetAutoScalingClientSpannerDatabaseName %s.",
      AutoScalingClientProto::ClientConfigurationKeys_Name(
          AutoScalingClientProto::ClientConfigurationKeys::
              CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_DATABASE_NAME)
          .c_str());
  return parameter;
}

void ConfigurationFetcher::GetAutoScalingClientScaleInHookName(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      AutoScalingClientProto::RequestConfigurationKeys_Name(
          AutoScalingClientProto::RequestConfigurationKeys::
              CMRT_AUTO_SCALING_CLIENT_SCALE_IN_HOOK_NAME),
      context);
  GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetAutoScalingClientScaleInHookNameSync(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetAutoScalingClientScaleInHookName, this,
               _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetAutoScalingClientScaleInHookName %s.",
      AutoScalingClientProto::RequestConfigurationKeys_Name(
          AutoScalingClientProto::RequestConfigurationKeys::
              CMRT_AUTO_SCALING_CLIENT_SCALE_IN_HOOK_NAME)
          .c_str());
  return parameter;
}

AsyncContext<string, string> ConfigurationFetcher::ContextConvertCallback(
    const string& parameter_name, AsyncContext<GetConfigurationRequest, string>&
                                      context_without_parameter_name) noexcept {
  return ConfigurationFetcherUtils::ContextConvertCallback<string>(
      parameter_name, context_without_parameter_name,
      [](const string& value) { return value; });
}

void ConfigurationFetcher::GetConfiguration(
    AsyncContext<string, string>& get_configuration_context) noexcept {
  if (environment_name_label_.empty()) {
    AsyncContext<GetConfigurationRequest, string> get_env_name_context(
        make_shared<GetConfigurationRequest>(), [](auto&) {},
        get_configuration_context);
    GetEnvironmentNameCallback(get_env_name_context, get_configuration_context);
    return;
  }
  auto get_env_name_context = AsyncContext<GetConfigurationRequest, string>(
      make_shared<GetConfigurationRequest>(),
      bind(&ConfigurationFetcher::GetEnvironmentNameCallback, this, _1,
           get_configuration_context),
      get_configuration_context);
  GetEnvironmentName(get_env_name_context);
}

void ConfigurationFetcher::GetEnvironmentNameCallback(
    AsyncContext<GetConfigurationRequest, string>& get_env_name_context,
    AsyncContext<string, string>& get_configuration_context) noexcept {
  string env_name;
  if (!environment_name_label_.empty()) {
    if (!get_env_name_context.result.Successful()) {
      get_configuration_context.result = get_env_name_context.result;
      SCP_ERROR_CONTEXT(kConfigurationFetcher, get_configuration_context,
                        get_configuration_context.result,
                        "Failed to GetEnvironmentName.");
      get_configuration_context.Finish();
      return;
    }
    env_name = *get_env_name_context.response + "-";
  }

  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(absl::StrCat(parameter_name_prefix_, env_name,
                                           *get_configuration_context.request));
  auto context = AsyncContext<GetParameterRequest, GetParameterResponse>(
      move(request),
      bind(&ConfigurationFetcher::GetParameterCallback, this, _1,
           get_configuration_context),
      get_configuration_context);
  parameter_client_->GetParameter(context);
}

void ConfigurationFetcher::GetParameterCallback(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        get_parameter_context,
    AsyncContext<string, string>& get_configuration_context) noexcept {
  if (!get_parameter_context.result.Successful()) {
    get_configuration_context.result = get_parameter_context.result;
    SCP_ERROR_CONTEXT(kConfigurationFetcher, get_configuration_context,
                      get_configuration_context.result,
                      "Failed to get parameter value for %s",
                      get_configuration_context.request->c_str());
    get_configuration_context.Finish();
    return;
  }

  get_configuration_context.result = SuccessExecutionResult();
  get_configuration_context.response = make_shared<string>(
      move(get_parameter_context.response->parameter_value()));
  get_configuration_context.Finish();
}
}  // namespace google::scp::cpio
