/*
 * Copyright 2024 Google LLC
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

#include "aws_kms_client.h"

#include <memory>
#include <string>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/kms_client_provider/src/aws/nontee_aws_kms_client_provider.h"
#include "cpio/client_providers/role_credentials_provider/src/aws/aws_role_credentials_provider.h"
#include "cpio/common/src/common_error_codes.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::AwsRoleCredentialsProvider;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::KmsClientProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderOptions;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;

namespace {
constexpr char kAwsKmsClient[] = "AwsKmsClient";
}  // namespace

namespace google::scp::cpio {

shared_ptr<RoleCredentialsProviderInterface>
AwsRoleCredentialsProviderFactory::Create(
    const string& region,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<client_providers::AuthTokenProviderInterface>&
        auth_token_provider) {
  auto role_credentials_provider_options =
      make_shared<RoleCredentialsProviderOptions>();
  role_credentials_provider_options->region = region;
  return make_shared<AwsRoleCredentialsProvider>(
      std::move(role_credentials_provider_options), instance_client_provider,
      cpu_async_executor, io_async_executor, auth_token_provider);
}

shared_ptr<KmsClientProviderInterface> AwsKmsClientProviderFactory::Create(
    const shared_ptr<AwsKmsClientOptions>& options,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_creadentials_provider,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor) {
  return make_shared<client_providers::NonteeAwsKmsClientProvider>(
      options, role_creadentials_provider, cpu_async_executor,
      io_async_executor);
}

ExecutionResult AwsKmsClient::Init() noexcept {
  shared_ptr<AsyncExecutorInterface> io_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor),
      kAwsKmsClient, kZeroUuid, "Failed to get IOAsyncExecutor.");
  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(cpu_async_executor),
      kAwsKmsClient, kZeroUuid, "Failed to get CpuAsyncExecutor.");

  shared_ptr<InstanceClientProviderInterface> instance_client_provider;
  auto aws_kms_client_options =
      std::dynamic_pointer_cast<AwsKmsClientOptions>(options_);
  if (!aws_kms_client_options) {
    auto execution_result = FailureExecutionResult(
        core::errors::SC_COMMON_ERRORS_POINTER_CASTING_FAILURE);
    SCP_ERROR(kAwsKmsClient, kZeroUuid, execution_result,
              "Cannot cast to AwsKmsClientOptions");
    return execution_result;
  }

  if (aws_kms_client_options->region.empty()) {
    RETURN_AND_LOG_IF_FAILURE(
        GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(
            instance_client_provider),
        kAwsKmsClient, kZeroUuid, "Failed to get InstanceClientProvider.");
  }

  shared_ptr<AuthTokenProviderInterface> auth_token_rovider;
  RETURN_AND_LOG_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetAuthTokenProvider(auth_token_rovider),
      kAwsKmsClient, kZeroUuid, "Failed to get AuthTokenProvider.");
  role_credentials_provider_ = role_credentials_provider_factory_->Create(
      aws_kms_client_options->region, instance_client_provider,
      cpu_async_executor, io_async_executor, auth_token_rovider);

  kms_client_provider_ = kms_client_provider_factory_->Create(
      aws_kms_client_options, role_credentials_provider_, cpu_async_executor,
      io_async_executor);

  RETURN_AND_LOG_IF_FAILURE(role_credentials_provider_->Init(), kAwsKmsClient,
                            kZeroUuid,
                            "Failed to init RoleCredentialProvider.");
  RETURN_AND_LOG_IF_FAILURE(kms_client_provider_->Init(), kAwsKmsClient,
                            kZeroUuid, "Failed to init KmsClientProvider.");

  return SuccessExecutionResult();
}

ExecutionResult AwsKmsClient::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(role_credentials_provider_->Run(), kAwsKmsClient,
                            kZeroUuid, "Failed to run RoleCredentialProvider.");
  RETURN_AND_LOG_IF_FAILURE(kms_client_provider_->Run(), kAwsKmsClient,
                            kZeroUuid, "Failed to run KmsClientProvider.");
  return SuccessExecutionResult();
}

ExecutionResult AwsKmsClient::Stop() noexcept {
  RETURN_AND_LOG_IF_FAILURE(kms_client_provider_->Stop(), kAwsKmsClient,
                            kZeroUuid, "Failed to stop KmsClientProvider.");
  RETURN_AND_LOG_IF_FAILURE(role_credentials_provider_->Stop(), kAwsKmsClient,
                            kZeroUuid,
                            "Failed to stop RoleCredentialProvider.");
  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio
