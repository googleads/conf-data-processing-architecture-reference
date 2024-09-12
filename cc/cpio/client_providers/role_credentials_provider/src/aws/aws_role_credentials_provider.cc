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

#include "aws_role_credentials_provider.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <aws/sts/model/AssumeRoleRequest.h>
#include <aws/sts/model/AssumeRoleWithWebIdentityRequest.h>

#include "cc/core/common/uuid/src/uuid.h"
#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/common/time_provider/src/time_provider.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_utils.h"
#include "cpio/client_providers/role_credentials_provider/src/aws/sts_error_converter.h"
#include "cpio/common/src/aws/aws_utils.h"

#include "error_codes.h"

using Aws::String;
using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::STS::STSClient;
using Aws::STS::Model::AssumeRoleOutcome;
using Aws::STS::Model::AssumeRoleRequest;
using Aws::STS::Model::AssumeRoleWithWebIdentityOutcome;
using Aws::STS::Model::AssumeRoleWithWebIdentityRequest;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::
    SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::errors::
    SC_AWS_ROLE_CREDENTIALS_PROVIDER_INVALID_REQUEST;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace {
constexpr char kAwsRoleCredentialsProvider[] = "AwsRoleCredentialsProvider";
constexpr char kGcpTokenTypeForAws[] = "LIMITED_AWS";
}  // namespace

namespace google::scp::cpio::client_providers {
shared_ptr<ClientConfiguration>
AwsRoleCredentialsProvider::CreateClientConfiguration(
    const string& region) noexcept {
  return common::CreateClientConfiguration(make_shared<string>(move(region)));
}

ExecutionResult AwsRoleCredentialsProvider::Init() noexcept {
  return SuccessExecutionResult();
};

ExecutionResult AwsRoleCredentialsProvider::Run() noexcept {
  if (!instance_client_provider_ && options_->region.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED);
    SCP_ERROR(kAwsRoleCredentialsProvider, kZeroUuid, execution_result,
              "InstanceClientProvider and region in the option cannot be both "
              "null or empty.");
    return execution_result;
  }

  if (!cpu_async_executor_ || !io_async_executor_) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED);
    SCP_ERROR(kAwsRoleCredentialsProvider, kZeroUuid, execution_result,
              "AsyncExecutor cannot be null.");
    return execution_result;
  }

  if (!auth_token_provider_) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED);
    SCP_ERROR(kAwsRoleCredentialsProvider, kZeroUuid, execution_result,
              "AuthTokenProvider cannot be null.");
    return execution_result;
  }

  string region;
  if (options_->region.empty()) {
    auto region_code_or =
        AwsInstanceClientUtils::GetCurrentRegionCode(instance_client_provider_);
    if (!region_code_or.Successful()) {
      SCP_ERROR(kAwsRoleCredentialsProvider, kZeroUuid, region_code_or.result(),
                "Failed to get region code for current instance");
      return region_code_or.result();
    }
    region = std::move(*region_code_or);
  } else {
    region = options_->region;
  }

  auto client_config = CreateClientConfiguration(region);
  client_config->executor = make_shared<AwsAsyncExecutor>(io_async_executor_);
  sts_client_ = make_shared<STSClient>(*client_config);

  auto timestamp =
      to_string(TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks());
  session_name_ = make_shared<string>(timestamp);

  return SuccessExecutionResult();
}

ExecutionResult AwsRoleCredentialsProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

void AwsRoleCredentialsProvider::GetRoleCredentials(
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context) noexcept {
  if (!get_credentials_context.request->account_identity ||
      get_credentials_context.request->account_identity->empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ROLE_CREDENTIALS_PROVIDER_INVALID_REQUEST);
    SCP_ERROR_CONTEXT(kAwsRoleCredentialsProvider, get_credentials_context,
                      execution_result, "Account identity is missing.");

    get_credentials_context.result = execution_result;
    get_credentials_context.Finish();
    return;
  }

  if (!get_credentials_context.request->target_audience_for_web_identity
           .empty()) {
    auto get_token_request = make_shared<GetTeeSessionTokenRequest>();
    get_token_request->token_type = make_shared<string>(kGcpTokenTypeForAws);
    get_token_request->token_target_audience_uri = make_shared<string>(
        get_credentials_context.request->target_audience_for_web_identity);
    AsyncContext<GetTeeSessionTokenRequest, GetSessionTokenResponse>
        get_token_context(std::move(get_token_request),
                          bind(&AwsRoleCredentialsProvider::OnGetTokenCallback,
                               this, get_credentials_context, _1),
                          get_credentials_context);

    auth_token_provider_->GetTeeSessionToken(get_token_context);
  } else {
    AssumeRoleRequest sts_request;
    sts_request.SetRoleArn(
        *(get_credentials_context.request->account_identity));
    sts_request.SetRoleSessionName(*session_name_);

    sts_client_->AssumeRoleAsync(
        sts_request,
        bind(&AwsRoleCredentialsProvider::OnGetRoleCredentialsCallback, this,
             get_credentials_context, _1, _2, _3, _4),
        nullptr);
  }
}

void AwsRoleCredentialsProvider::OnGetTokenCallback(
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context,
    AsyncContext<GetTeeSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsRoleCredentialsProvider, get_credentials_context,
                      get_token_context.result, "Failed to get token.");

    get_credentials_context.result = get_token_context.result;
    get_credentials_context.Finish();
    return;
  }

  AssumeRoleWithWebIdentityRequest sts_request;
  sts_request.SetRoleArn(*(get_credentials_context.request->account_identity));
  sts_request.SetRoleSessionName(*session_name_);
  sts_request.SetWebIdentityToken(*get_token_context.response->session_token);

  sts_client_->AssumeRoleWithWebIdentityAsync(
      sts_request,
      bind(&AwsRoleCredentialsProvider::
               OnGetRoleCredentialsWithWebIdentityCallback,
           this, get_credentials_context, _1, _2, _3, _4),
      nullptr);
}

void AwsRoleCredentialsProvider::OnGetRoleCredentialsCallback(
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context,
    const STSClient* sts_client,
    const AssumeRoleRequest& get_credentials_request,
    const AssumeRoleOutcome& get_credentials_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!get_credentials_outcome.IsSuccess()) {
    auto execution_result = STSErrorConverter::ConvertSTSError(
        get_credentials_outcome.GetError().GetErrorType(),
        get_credentials_outcome.GetError().GetMessage());

    get_credentials_context.result = execution_result;

    // Retries for retriable errors with high priority if specified in the
    // callback of get_credentials_context.
    if (!cpu_async_executor_
             ->Schedule(
                 [get_credentials_context]() mutable {
                   get_credentials_context.Finish();
                 },
                 AsyncPriority::High)
             .Successful()) {
      get_credentials_context.Finish();
    }
    return;
  }

  get_credentials_context.result = SuccessExecutionResult();
  get_credentials_context.response = make_shared<GetRoleCredentialsResponse>();
  get_credentials_context.response->access_key_id =
      make_shared<string>(get_credentials_outcome.GetResult()
                              .GetCredentials()
                              .GetAccessKeyId()
                              .c_str());
  get_credentials_context.response->access_key_secret =
      make_shared<string>(get_credentials_outcome.GetResult()
                              .GetCredentials()
                              .GetSecretAccessKey()
                              .c_str());
  get_credentials_context.response->security_token =
      make_shared<string>(get_credentials_outcome.GetResult()
                              .GetCredentials()
                              .GetSessionToken()
                              .c_str());

  get_credentials_context.Finish();
}

void AwsRoleCredentialsProvider::OnGetRoleCredentialsWithWebIdentityCallback(
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context,
    const STSClient* sts_client,
    const AssumeRoleWithWebIdentityRequest& get_credentials_request,
    const AssumeRoleWithWebIdentityOutcome& get_credentials_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!get_credentials_outcome.IsSuccess()) {
    auto execution_result = STSErrorConverter::ConvertSTSError(
        get_credentials_outcome.GetError().GetErrorType(),
        get_credentials_outcome.GetError().GetMessage());

    get_credentials_context.result = execution_result;

    // Retries for retriable errors with high priority if specified in the
    // callback of get_credentials_context.
    if (!cpu_async_executor_
             ->Schedule(
                 [get_credentials_context]() mutable {
                   get_credentials_context.Finish();
                 },
                 AsyncPriority::High)
             .Successful()) {
      get_credentials_context.Finish();
    }
    return;
  }

  get_credentials_context.result = SuccessExecutionResult();
  get_credentials_context.response = make_shared<GetRoleCredentialsResponse>();
  get_credentials_context.response->access_key_id =
      make_shared<string>(get_credentials_outcome.GetResult()
                              .GetCredentials()
                              .GetAccessKeyId()
                              .c_str());
  get_credentials_context.response->access_key_secret =
      make_shared<string>(get_credentials_outcome.GetResult()
                              .GetCredentials()
                              .GetSecretAccessKey()
                              .c_str());
  get_credentials_context.response->security_token =
      make_shared<string>(get_credentials_outcome.GetResult()
                              .GetCredentials()
                              .GetSessionToken()
                              .c_str());

  get_credentials_context.Finish();
}

#ifndef TEST_CPIO
std::shared_ptr<RoleCredentialsProviderInterface>
RoleCredentialsProviderFactory::Create(
    const shared_ptr<RoleCredentialsProviderOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
    const shared_ptr<AuthTokenProviderInterface>&
        auth_token_provider) noexcept {
  return make_shared<AwsRoleCredentialsProvider>(
      options, instance_client_provider, cpu_async_executor, io_async_executor,
      auth_token_provider);
}
#endif
}  // namespace google::scp::cpio::client_providers
