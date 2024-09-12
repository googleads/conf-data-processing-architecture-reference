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

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/http_client_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/instance_client/instance_client_interface.h"
#include "public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "public/cpio/utils/configuration_fetcher/interface/configuration_fetcher_interface.h"

#include "error_codes.h"

namespace google::scp::cpio {
static constexpr char kDefaultParameterNamePrefix[] = "scp-";
static constexpr char kDefaultEnvironmentNameLabel[] = "environment";

/*! @copydoc ConfigurationFetcherInterface
 */
class ConfigurationFetcher : public ConfigurationFetcherInterface {
 public:
  /**
   * @brief Construct a new Configuration Fetcher object. If the parameter name
   * prefix and the environment name label are not changed in the terraform,
   * don't specify them and use the default ones.
   *
   * The InstanceClient and ParameterClient will be created inside
   * ConfigurationFetcher because to create them, we need the AsyncExecutor
   * whose configuration can be fetched through ConfigurationFetcher. This will
   * cause a cyclic dependency.
   *
   * If using this constructor, the InstanceClient and ParameterClient used here
   * will not be the same as what the customer creates and uses outside.
   *
   * @param parameter_name_prefix optional parameter name prefix. If it is not
   * present, use the default prefix. If there is no prefix, input empty
   * string.
   * @param environment_name_label optional environment name label. If it is not
   * present, use the default label. If there is no environment name label,
   * input empty string.
   */
  explicit ConfigurationFetcher(
      std::optional<std::string> parameter_name_prefix,
      std::optional<std::string> environment_name_label)
      : parameter_name_prefix_(
            parameter_name_prefix.value_or(kDefaultParameterNamePrefix)),
        environment_name_label_(
            environment_name_label.value_or(kDefaultEnvironmentNameLabel)) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResultOr<std::string> GetCurrentInstanceResourceNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetCurrentInstanceResourceName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetEnvironmentNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetEnvironmentName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetParameterByNameSync(
      std::string parameter_name) noexcept override;

  void GetParameterByName(
      core::AsyncContext<std::string, std::string> context) noexcept override;

  core::ExecutionResultOr<uint64_t> GetUInt64ByNameSync(
      std::string parameter_name) noexcept override;

  void GetUInt64ByName(
      core::AsyncContext<std::string, uint64_t> context) noexcept override;

  core::ExecutionResultOr<LogOption> GetCommonLogOptionSync(
      GetConfigurationRequest request) noexcept override;

  void GetCommonLogOption(core::AsyncContext<GetConfigurationRequest, LogOption>
                              context) noexcept override;

  core::ExecutionResultOr<std::unordered_set<core::LogLevel>>
  GetCommonEnabledLogLevelsSync(
      GetConfigurationRequest request) noexcept override;

  void GetCommonEnabledLogLevels(
      core::AsyncContext<GetConfigurationRequest,
                         std::unordered_set<core::LogLevel>>
          context) noexcept override;

  core::ExecutionResultOr<size_t> GetCommonCpuThreadCountSync(
      GetConfigurationRequest request) noexcept override;

  void GetCommonCpuThreadCount(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetCommonCpuThreadPoolQueueCapSync(
      GetConfigurationRequest request) noexcept override;

  void GetCommonCpuThreadPoolQueueCap(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetCommonIoThreadCountSync(
      GetConfigurationRequest request) noexcept override;

  void GetCommonIoThreadCount(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetCommonIoThreadPoolQueueCapSync(
      GetConfigurationRequest request) noexcept override;

  void GetCommonIoThreadPoolQueueCap(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetJobLifecycleHelperRetryLimitSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobLifecycleHelperRetryLimit(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t>
  GetJobLifecycleHelperVisibilityTimeoutExtendTimeSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobLifecycleHelperVisibilityTimeoutExtendTime(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetJobLifecycleHelperJobProcessingTimeoutSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobLifecycleHelperJobProcessingTimeout(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t>
  GetJobLifecycleHelperJobExtendingWorkerSleepTimeSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobLifecycleHelperJobExtendingWorkerSleepTime(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<bool> GetJobLifecycleHelperEnableMetricRecordingSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobLifecycleHelperEnableMetricRecording(
      core::AsyncContext<GetConfigurationRequest, bool> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetJobLifecycleHelperMetricNamespaceSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobLifecycleHelperMetricNamespace(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetJobClientJobQueueNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobClientJobQueueName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetJobClientJobTableNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobClientJobTableName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetGcpJobClientSpannerInstanceNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetGcpJobClientSpannerInstanceName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetGcpJobClientSpannerDatabaseNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetGcpJobClientSpannerDatabaseName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;
  core::ExecutionResultOr<size_t> GetJobClientReadJobRetryIntervalSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobClientReadJobRetryInterval(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetJobClientReadJobMaxRetryCountSync(
      GetConfigurationRequest request) noexcept override;

  void GetJobClientReadJobMaxRetryCount(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<std::string>
  GetGcpNoSQLDatabaseClientSpannerInstanceNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetGcpNoSQLDatabaseClientSpannerInstanceName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string>
  GetGcpNoSQLDatabaseClientSpannerDatabaseNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetGcpNoSQLDatabaseClientSpannerDatabaseName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetQueueClientQueueNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetQueueClientQueueName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  void GetMetricClientEnableBatchRecording(
      core::AsyncContext<GetConfigurationRequest, bool> context) noexcept
      override;

  core::ExecutionResultOr<bool> GetMetricClientEnableBatchRecordingSync(
      GetConfigurationRequest request) noexcept override;

  void GetMetricClientNamespaceForBatchRecording(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string>
  GetMetricClientNamespaceForBatchRecordingSync(
      GetConfigurationRequest request) noexcept override;

  void GetMetricClientBatchRecordingTimeDurationInMs(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t>
  GetMetricClientBatchRecordingTimeDurationInMsSync(
      GetConfigurationRequest request) noexcept override;

  void GetAutoScalingClientInstanceTableName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string>
  GetAutoScalingClientInstanceTableNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetAutoScalingClientSpannerInstanceName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string>
  GetAutoScalingClientSpannerInstanceNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetAutoScalingClientSpannerDatabaseName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string>
  GetAutoScalingClientSpannerDatabaseNameSync(
      GetConfigurationRequest request) noexcept override;

  void GetAutoScalingClientScaleInHookName(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetAutoScalingClientScaleInHookNameSync(
      GetConfigurationRequest request) noexcept override;

 protected:
  virtual void CreateInstanceAndParameterClient() noexcept;
  std::shared_ptr<InstanceClientInterface> instance_client_;
  std::shared_ptr<ParameterClientInterface> parameter_client_;

 private:
  core::AsyncContext<std::string, std::string> ContextConvertCallback(
      const std::string& parameter_name,
      core::AsyncContext<GetConfigurationRequest, std::string>&
          context_without_parameter_name) noexcept;

  void GetConfiguration(core::AsyncContext<std::string, std::string>&
                            get_configuration_context) noexcept;

  void GetCurrentInstanceResourceNameCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          get_current_instance_response_name_context,
      core::AsyncContext<GetConfigurationRequest, std::string>&
          context) noexcept;

  void GetCurrentInstanceResourceNameForEnvNameCallback(
      core::AsyncContext<GetConfigurationRequest, std::string>&
          get_current_instance_resource_name_context,
      core::AsyncContext<GetConfigurationRequest, std::string>&
          get_env_name_context) noexcept;

  void GetInstanceDetailsByResourceNameCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          get_instance_details_context,
      core::AsyncContext<GetConfigurationRequest, std::string>&
          get_env_name_context) noexcept;

  void GetEnvironmentNameCallback(
      core::AsyncContext<GetConfigurationRequest, std::string>&
          get_env_name_context,
      core::AsyncContext<std::string, std::string>&
          get_configuration_context) noexcept;

  void GetParameterCallback(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          get_parameter_context,
      core::AsyncContext<std::string, std::string>&
          get_configuration_context) noexcept;

  std::string parameter_name_prefix_;
  std::string environment_name_label_;

  // Needed to construct InstanceClient and ParameterClient.
  std::shared_ptr<core::HttpClientInterface> http1_client_, http2_client_;
  std::shared_ptr<client_providers::AuthTokenProviderInterface>
      auth_token_provider_;
  std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;
  std::shared_ptr<client_providers::InstanceClientProviderInterface>
      instance_client_provider_;
};
}  // namespace google::scp::cpio
