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

#include <string>
#include <unordered_set>

#include "core/interface/async_context.h"
#include "core/interface/logger_interface.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio {
/// Request to get configuration. Currently it is empty and it is just a place
/// holder in AsyncContext.
struct GetConfigurationRequest {
  virtual ~GetConfigurationRequest() = default;
};

/**
 * @brief Helper to fetch configurations.
 */
class ConfigurationFetcherInterface : public core::ServiceInterface {
 public:
  virtual ~ConfigurationFetcherInterface() = default;

  /**
   * @brief Get currrent instance resource name.
   *
   * @return core::ExecutionResultOr<std::string> the instance resource name.
   */
  virtual core::ExecutionResultOr<std::string>
  GetCurrentInstanceResourceNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get currrent instance resource name.
   *
   * @param context the async context for the operation.
   */
  virtual void GetCurrentInstanceResourceName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get environment name stoared as the instance tag
   * environment_name_tag.
   *
   * @return core::ExecutionResultOr<std::string> the instance resource name.
   */
  virtual core::ExecutionResultOr<std::string> GetEnvironmentNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get environment name stoared as the instance tag
   * environment_name_tag.
   *
   * @param context the async context for the operation.
   */
  virtual void GetEnvironmentName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get parameter by name.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the parameter and
   * result.
   */
  virtual core::ExecutionResultOr<std::string> GetParameterByNameSync(
      std::string parameter_name) noexcept = 0;

  /**
   * @brief Get parameter by name.
   *
   * @param context the async context for the operation.
   */
  virtual void GetParameterByName(
      core::AsyncContext<std::string, std::string> context) noexcept = 0;

  /**
   * @brief Gets a parameter by name and converts it into a uint64.
   *
   * @param parameter_name Name of the parameter to get.
   * @return core::ExecutionResultOr<uint64_t> the parameter or result.
   */
  virtual core::ExecutionResultOr<uint64_t> GetUInt64ByNameSync(
      std::string parameter_name) noexcept = 0;

  /**
   * @brief Get parameter by name and converts it into a uint64.
   *
   * @param context the async context for the operation.
   */
  virtual void GetUInt64ByName(
      core::AsyncContext<std::string, uint64_t> context) noexcept = 0;

  /**** Shared configurations start */
  /**
   * @brief Get common LogOption.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<LogOption> the common LogOption and result.
   */
  virtual core::ExecutionResultOr<LogOption> GetCommonLogOptionSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get common LogOption.
   *
   * @param context the async context for the operation.
   */
  virtual void GetCommonLogOption(
      core::AsyncContext<GetConfigurationRequest, LogOption>
          context) noexcept = 0;

  /**
   * @brief Get common EnabledLogLevels.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::unordered_set<core::LogLevel>> the
   * common enabled log levels and result.
   */
  virtual core::ExecutionResultOr<std::unordered_set<core::LogLevel>>
  GetCommonEnabledLogLevelsSync(GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get common EnabledLogLevels.
   *
   * @param context the async context for the operation.
   */
  virtual void GetCommonEnabledLogLevels(
      core::AsyncContext<GetConfigurationRequest,
                         std::unordered_set<core::LogLevel>>
          context) noexcept = 0;

  /**
   * @brief Get the CPU thread count for the shared CPU thread pool.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the CPU thread count and result.
   */
  virtual core::ExecutionResultOr<size_t> GetCommonCpuThreadCountSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the CPU thread count for the shared CPU thread pool.
   *
   * @param context the async context for the operation.
   */
  virtual void GetCommonCpuThreadCount(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the queue cap for the shared CPU thread pool.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the queue cap and result.
   */
  virtual core::ExecutionResultOr<size_t> GetCommonCpuThreadPoolQueueCapSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the queue cap for the shared CPU thread pool.
   *
   * @param context the async context for the operation.
   */
  virtual void GetCommonCpuThreadPoolQueueCap(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the IO thread count for the shared IO thread pool.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the IO thread count and result.
   */
  virtual core::ExecutionResultOr<size_t> GetCommonIoThreadCountSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the IO thread count for the shared IO thread pool.
   *
   * @param context the async context for the operation.
   */
  virtual void GetCommonIoThreadCount(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the queue cap for the shared Io thread pool.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the queue cap and result.
   */
  virtual core::ExecutionResultOr<size_t> GetCommonIoThreadPoolQueueCapSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the queue cap for the shared IO thread pool.
   *
   * @param context the async context for the operation.
   */
  virtual void GetCommonIoThreadPoolQueueCap(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**** Shared configurations end */

  /**** JobLifecycleHelper configurations start */

  /**
   * @brief Get the Job Lifecycle Helper Retry Limit value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the Retry Limit and result.
   */
  virtual core::ExecutionResultOr<size_t> GetJobLifecycleHelperRetryLimitSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Retry Limit value
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobLifecycleHelperRetryLimit(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Visibility Timeout Extend Time.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the Visibility Timeout Extend Time
   * and result.
   */
  virtual core::ExecutionResultOr<size_t>
  GetJobLifecycleHelperVisibilityTimeoutExtendTimeSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Visibility Timeout Extend Time
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobLifecycleHelperVisibilityTimeoutExtendTime(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Job Processing Timeout.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the Job Processing Timeout and
   * result.
   */
  virtual core::ExecutionResultOr<size_t>
  GetJobLifecycleHelperJobProcessingTimeoutSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Job Processing Timeout
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobLifecycleHelperJobProcessingTimeout(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Job Extending Worker Sleep Time.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the Job Extending Worker Sleep Time
   * and result.
   */
  virtual core::ExecutionResultOr<size_t>
  GetJobLifecycleHelperJobExtendingWorkerSleepTimeSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Job Extending Worker Sleep Time
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobLifecycleHelperJobExtendingWorkerSleepTime(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Enable Metric Recording.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<bool> the Enable Metric Recording and
   * result.
   */
  virtual core::ExecutionResultOr<bool>
  GetJobLifecycleHelperEnableMetricRecordingSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Enable Metric Recording
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobLifecycleHelperEnableMetricRecording(
      core::AsyncContext<GetConfigurationRequest, bool> context) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Metric Namespace.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the Metric Namespace and
   * result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetJobLifecycleHelperMetricNamespaceSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Lifecycle Helper Metric Namespace
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobLifecycleHelperMetricNamespace(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**** JobLifecycleHelper configurations end */

  /**** JobClient configurations start */
  /**
   * @brief Get the Job Client Job Queue Name value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the Job Queue
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string> GetJobClientJobQueueNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client Job Queue Name value
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobClientJobQueueName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get the Job Client Job Table Name value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the Job Table
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string> GetJobClientJobTableNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client Job Table Name value
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobClientJobTableName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get the Job Client GCP Job Spanner Instance Name value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the GCP Job Spanner Instance
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetGcpJobClientSpannerInstanceNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client GCP Job Spanner Instance Name value
   *
   * @param context the async context for the operation.
   */
  virtual void GetGcpJobClientSpannerInstanceName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get the Job Client GCP Job Spanner Database Name value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the GCP Job Spanner Database
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetGcpJobClientSpannerDatabaseNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client GCP Job Spanner Database Name value
   *
   * @param context the async context for the operation.
   */
  virtual void GetGcpJobClientSpannerDatabaseName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get the Job Client read job retry interval.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the retry interval.
   */
  virtual core::ExecutionResultOr<size_t> GetJobClientReadJobRetryIntervalSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client read job retry interval.
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobClientReadJobRetryInterval(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the Job Client read job max retry count.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the max retry count.
   */
  virtual core::ExecutionResultOr<size_t> GetJobClientReadJobMaxRetryCountSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client read job max retry count.
   *
   * @param context the async context for the operation.
   */
  virtual void GetJobClientReadJobMaxRetryCount(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**** JobClient configurations end */

  /**** NoSQLDatabaseClient configurations start */
  /**
   * @brief Get the GcpNoSQLDatabaseClientSpannerInstanceName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the name and result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetGcpNoSQLDatabaseClientSpannerInstanceNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the GcpNoSQLDatabaseClientSpannerInstanceName.
   *
   * @param context the async context for the operation.
   */
  virtual void GetGcpNoSQLDatabaseClientSpannerInstanceName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;
  /**
   * @brief Get the GcpNoSQLDatabaseClientSpannerDatabaseName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the name and result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetGcpNoSQLDatabaseClientSpannerDatabaseNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the GcpNoSQLDatabaseClientSpannerDatabaseName.
   *
   * @param context the async context for the operation.
   */
  virtual void GetGcpNoSQLDatabaseClientSpannerDatabaseName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**** NoSQLDatabaseClient configurations end */

  /**** QueueClient configurations start */
  /**
   * @brief Get QueueClientQueueName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the Job Queue
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string> GetQueueClientQueueNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get QueueClientQueueName.
   *
   * @param context the async context for the operation.
   */
  virtual void GetQueueClientQueueName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;
  /**** QueueClient configurations end */

  /**** MetricClient configurations start */
  /**
   * @brief Get MetricClientEnableBatchRecording.
   *
   * @param context the async context for the operation.
   */
  virtual void GetMetricClientEnableBatchRecording(
      core::AsyncContext<GetConfigurationRequest, bool> context) noexcept = 0;

  /**
   * @brief Get MetricClientEnableBatchRecording.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<bool> the result.
   */
  virtual core::ExecutionResultOr<bool> GetMetricClientEnableBatchRecordingSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get MetricClientNamespaceForBatchRecording.
   *
   * @param context the async context for the operation.
   */
  virtual void GetMetricClientNamespaceForBatchRecording(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get MetricClientNamespaceForBatchRecording.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetMetricClientNamespaceForBatchRecordingSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get MetricClientBatchRecordingTimeDurationInMs.
   *
   * @param context the async context for the operation.
   */
  virtual void GetMetricClientBatchRecordingTimeDurationInMs(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get MetricClientBatchRecordingTimeDurationInMs.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<size_t> the result.
   */
  virtual core::ExecutionResultOr<size_t>
  GetMetricClientBatchRecordingTimeDurationInMsSync(
      GetConfigurationRequest request) noexcept = 0;
  /**** MetricClient configurations end */

  /**** AutoScalingClient configurations start */
  /**
   * @brief Get AutoScalingClientInstanceTableName.
   *
   * @param context the async context for the operation.
   */
  virtual void GetAutoScalingClientInstanceTableName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get AutoScalingClientInstanceTableName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetAutoScalingClientInstanceTableNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get AutoScalingClientSpannerInstanceName.
   *
   * @param context the async context for the operation.
   */
  virtual void GetAutoScalingClientSpannerInstanceName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get AutoScalingClientSpannerInstanceName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetAutoScalingClientSpannerInstanceNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get AutoScalingClientSpannerDatabaseName.
   *
   * @param context the async context for the operation.
   */
  virtual void GetAutoScalingClientSpannerDatabaseName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get AutoScalingClientSpannerDatabaseName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetAutoScalingClientSpannerDatabaseNameSync(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get AutoScalingClientScaleInHookName.
   *
   * @param context the async context for the operation.
   */
  virtual void GetAutoScalingClientScaleInHookName(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get AutoScalingClientScaleInHookName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetAutoScalingClientScaleInHookNameSync(
      GetConfigurationRequest request) noexcept = 0;
  /**** AutoScalingClient configurations end */
};

}  // namespace google::scp::cpio
