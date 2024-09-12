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

#include <gmock/gmock.h>

#include <string>
#include <unordered_set>

#include "core/interface/async_context.h"
#include "core/interface/logger_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/utils/configuration_fetcher/interface/configuration_fetcher_interface.h"

namespace google::scp::cpio {
class MockConfigurationFetcher
    : public testing::NiceMock<ConfigurationFetcherInterface> {
 public:
  MockConfigurationFetcher() {
    ON_CALL(*this, Init)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
  }

  MOCK_METHOD(core::ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Run, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>, GetEnvironmentNameSync,
              (GetConfigurationRequest), (noexcept, override));

  MOCK_METHOD(void, GetEnvironmentName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetCurrentInstanceResourceNameSync, (GetConfigurationRequest),
              (noexcept, override));

  MOCK_METHOD(void, GetCurrentInstanceResourceName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>, GetParameterByNameSync,
              ((std::string)), (noexcept, override));

  MOCK_METHOD(void, GetParameterByName,
              ((core::AsyncContext<std::string, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<uint64_t>, GetUInt64ByNameSync,
              (std::string), (noexcept, override));

  MOCK_METHOD(void, GetUInt64ByName,
              ((core::AsyncContext<std::string, uint64_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<LogOption>, GetCommonLogOptionSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetCommonLogOption,
              ((core::AsyncContext<GetConfigurationRequest, LogOption>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::unordered_set<core::LogLevel>>,
              GetCommonEnabledLogLevelsSync, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetCommonEnabledLogLevels,
              ((core::AsyncContext<GetConfigurationRequest,
                                   std::unordered_set<core::LogLevel>>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>, GetCommonCpuThreadCountSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetCommonCpuThreadCount,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetCommonCpuThreadPoolQueueCapSync, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetCommonCpuThreadPoolQueueCap,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>, GetCommonIoThreadCountSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetCommonIoThreadCount,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetCommonIoThreadPoolQueueCapSync, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetCommonIoThreadPoolQueueCap,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetJobLifecycleHelperRetryLimitSync, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetJobLifecycleHelperRetryLimit,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetJobLifecycleHelperVisibilityTimeoutExtendTimeSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetJobLifecycleHelperVisibilityTimeoutExtendTime,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetJobLifecycleHelperJobProcessingTimeoutSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetJobLifecycleHelperJobProcessingTimeout,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetJobLifecycleHelperJobExtendingWorkerSleepTimeSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetJobLifecycleHelperJobExtendingWorkerSleepTime,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<bool>,
              GetJobLifecycleHelperEnableMetricRecordingSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetJobLifecycleHelperEnableMetricRecording,
              ((core::AsyncContext<GetConfigurationRequest, bool>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetJobLifecycleHelperMetricNamespaceSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetJobLifecycleHelperMetricNamespace,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetJobClientJobQueueNameSync, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetJobClientJobQueueName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetJobClientJobTableNameSync, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetJobClientJobTableName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetGcpJobClientSpannerInstanceNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetGcpJobClientSpannerInstanceName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetGcpJobClientSpannerDatabaseNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetGcpJobClientSpannerDatabaseName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetJobClientReadJobRetryIntervalSync, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetJobClientReadJobRetryInterval,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetJobClientReadJobMaxRetryCountSync, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(void, GetJobClientReadJobMaxRetryCount,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetGcpNoSQLDatabaseClientSpannerInstanceNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetGcpNoSQLDatabaseClientSpannerInstanceName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetGcpNoSQLDatabaseClientSpannerDatabaseNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetGcpNoSQLDatabaseClientSpannerDatabaseName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>, GetQueueClientQueueNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetQueueClientQueueName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<bool>,
              GetMetricClientEnableBatchRecordingSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetMetricClientEnableBatchRecording,
              ((core::AsyncContext<GetConfigurationRequest, bool>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetMetricClientNamespaceForBatchRecordingSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetMetricClientNamespaceForBatchRecording,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>,
              GetMetricClientBatchRecordingTimeDurationInMsSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetMetricClientBatchRecordingTimeDurationInMs,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetAutoScalingClientInstanceTableNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetAutoScalingClientInstanceTableName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetAutoScalingClientSpannerInstanceNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetAutoScalingClientSpannerInstanceName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetAutoScalingClientSpannerDatabaseNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetAutoScalingClientSpannerDatabaseName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetAutoScalingClientScaleInHookNameSync,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(void, GetAutoScalingClientScaleInHookName,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));
};
}  // namespace google::scp::cpio
