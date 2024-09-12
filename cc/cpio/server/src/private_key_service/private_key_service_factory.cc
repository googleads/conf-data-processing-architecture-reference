/*
 * Copyright 2022 Google LLC
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

#include "private_key_service_factory.h"

#include <list>
#include <memory>
#include <string>

#include "cc/core/common/uuid/src/uuid.h"
#include "core/async_executor/src/async_executor.h"
#include "core/curl_client/src/http1_curl_client.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/config_provider_interface.h"
#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "cpio/client_providers/private_key_client_provider/src/private_key_client_provider.h"
#include "cpio/server/interface/private_key_service/private_key_service_factory_interface.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/proto/private_key_service/v1/configuration_keys.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::private_key_service::v1::ClientConfigurationKeys;
using google::cmrt::sdk::private_key_service::v1::ClientConfigurationKeys_Name;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Http1CurlClient;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_SERVICE_MISMATCHED_SECONDARY_ENDPOINTS;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::PrivateKeyClientProvider;
using google::scp::cpio::client_providers::PrivateKeyClientProviderInterface;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace {
constexpr char kPrivateKeyServiceFactory[] = "PrivateKeyServiceFactory";
constexpr int kDefaultCpuThreadCount = 2;
constexpr int kDefaultCpuThreadPoolQueueCap = 100000;
constexpr int kDefaultIoThreadCount = 2;
constexpr int kDefaultIoThreadPoolQueueCap = 100000;
}  // namespace

namespace google::scp::cpio {
ExecutionResult PrivateKeyServiceFactory::ReadConfigurations() noexcept {
  client_options_ = CreatePrivateKeyClientOptions();

  return SuccessExecutionResult();
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
PrivateKeyServiceFactory::CreateCpuAsyncExecutor() noexcept {
  int32_t cpu_thread_count = kDefaultCpuThreadCount;
  TryReadConfigInt(
      config_provider_,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::CMRT_PRIVATE_KEY_CLIENT_CPU_THREAD_COUNT),
      cpu_thread_count);

  int32_t cpu_queue_cap = kDefaultCpuThreadPoolQueueCap;
  TryReadConfigInt(config_provider_,
                   ClientConfigurationKeys_Name(
                       ClientConfigurationKeys::
                           CMRT_PRIVATE_KEY_CLIENT_CPU_THREAD_POOL_QUEUE_CAP),
                   cpu_queue_cap);
  cpu_async_executor_ =
      make_shared<AsyncExecutor>(cpu_thread_count, cpu_queue_cap);
  return cpu_async_executor_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
PrivateKeyServiceFactory::CreateIoAsyncExecutor() noexcept {
  int32_t io_thread_count = kDefaultIoThreadCount;
  TryReadConfigInt(
      config_provider_,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::CMRT_PRIVATE_KEY_CLIENT_IO_THREAD_COUNT),
      io_thread_count);

  int32_t io_queue_cap = kDefaultIoThreadPoolQueueCap;
  TryReadConfigInt(config_provider_,
                   ClientConfigurationKeys_Name(
                       ClientConfigurationKeys::
                           CMRT_PRIVATE_KEY_CLIENT_IO_THREAD_POOL_QUEUE_CAP),
                   io_queue_cap);
  io_async_executor_ =
      make_shared<AsyncExecutor>(io_thread_count, io_queue_cap);
  return io_async_executor_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
PrivateKeyServiceFactory::CreateHttp1Client() noexcept {
  http1_client_ =
      make_shared<Http1CurlClient>(cpu_async_executor_, io_async_executor_);
  return http1_client_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
PrivateKeyServiceFactory::CreateHttp2Client() noexcept {
  http2_client_ = make_shared<HttpClient>(cpu_async_executor_);
  return http2_client_;
}

shared_ptr<PrivateKeyClientOptions>
PrivateKeyServiceFactory::CreatePrivateKeyClientOptions() noexcept {
  return make_shared<PrivateKeyClientOptions>();
}

ExecutionResult PrivateKeyServiceFactory::Init() noexcept {
  RETURN_AND_LOG_IF_FAILURE(ReadConfigurations(), kPrivateKeyServiceFactory,
                            kZeroUuid, "Failed to read configurations");

  RETURN_AND_LOG_IF_FAILURE(component_factory_->Init(),
                            kPrivateKeyServiceFactory, kZeroUuid,
                            "Failed to init component factory.");

  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyServiceFactory::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(component_factory_->Run(),
                            kPrivateKeyServiceFactory, kZeroUuid,
                            "Failed to run component factory.");
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyServiceFactory::Stop() noexcept {
  RETURN_AND_LOG_IF_FAILURE(component_factory_->Stop(),
                            kPrivateKeyServiceFactory, kZeroUuid,
                            "Failed to stop component factory.");
  return SuccessExecutionResult();
}

shared_ptr<PrivateKeyClientProviderInterface>
PrivateKeyServiceFactory::CreatePrivateKeyClient() noexcept {
  return make_shared<PrivateKeyClientProvider>(
      client_options_, http2_client_, private_key_fetcher_, kms_client_);
}
}  // namespace google::scp::cpio
