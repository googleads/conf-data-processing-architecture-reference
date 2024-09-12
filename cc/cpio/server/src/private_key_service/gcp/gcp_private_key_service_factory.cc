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

#include "gcp_private_key_service_factory.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cc/core/common/uuid/src/uuid.h"
#include "cpio/client_providers/auth_token_provider/src/gcp/gcp_auth_token_provider.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "cpio/client_providers/kms_client_provider/src/gcp/gcp_kms_client_provider.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/gcp/gcp_private_key_fetcher_provider.h"
#include "cpio/server/src/component_factory/component_factory.h"
#include "cpio/server/src/private_key_service/private_key_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/proto/private_key_service/v1/configuration_keys.pb.h"

using google::cmrt::sdk::private_key_service::v1::ClientConfigurationKeys;
using google::cmrt::sdk::private_key_service::v1::ClientConfigurationKeys_Name;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_SERVICE_MISMATCHED_SECONDARY_ENDPOINTS;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::GcpAuthTokenProvider;
using google::scp::cpio::client_providers::GcpKmsClientProvider;
using google::scp::cpio::client_providers::GcpPrivateKeyFetcherProvider;
using google::scp::cpio::client_providers::KmsClientProviderInterface;
using google::scp::cpio::client_providers::PrivateKeyFetcherProviderInterface;
using std::bind;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {
constexpr char kGcpPrivateKeyServiceFactory[] = "GcpPrivateKeyServiceFactory";
}  // namespace

namespace google::scp::cpio {
ExecutionResultOr<shared_ptr<ServiceInterface>>
GcpPrivateKeyServiceFactory::CreateAuthTokenProvider() noexcept {
  auth_token_provider_ =
      make_shared<GcpAuthTokenProvider>(http1_client_, io_async_executor_);
  return auth_token_provider_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
GcpPrivateKeyServiceFactory::CreatePrivateKeyFetcher() noexcept {
  private_key_fetcher_ = make_shared<GcpPrivateKeyFetcherProvider>(
      http2_client_, auth_token_provider_);
  return private_key_fetcher_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
GcpPrivateKeyServiceFactory::CreateKmsClient() noexcept {
  kms_client_ = make_shared<GcpKmsClientProvider>(io_async_executor_,
                                                  cpu_async_executor_);
  return kms_client_;
}

ExecutionResult GcpPrivateKeyServiceFactory::Init() noexcept {
  vector<ComponentCreator> creators(
      {ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateIoAsyncExecutor, this),
           "IoAsyncExecutor"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateCpuAsyncExecutor, this),
           "CpuAsyncExecutor"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateHttp1Client, this),
           "Http1Client"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateHttp2Client, this),
           "Http2Client"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateAuthTokenProvider, this),
           "AuthTokenProvider"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreatePrivateKeyFetcher, this),
           "PrivateKeyFetcher"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateKmsClient, this),
           "KmsClient")});
  component_factory_ = make_shared<ComponentFactory>(move(creators));

  RETURN_AND_LOG_IF_FAILURE(PrivateKeyServiceFactory::Init(),
                            kGcpPrivateKeyServiceFactory, kZeroUuid,
                            "Failed to init PrivateKeyServiceFactory.");

  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio
