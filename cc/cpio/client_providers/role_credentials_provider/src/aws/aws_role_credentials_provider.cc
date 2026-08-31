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

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/sts/model/AssumeRoleRequest.h>
#include <aws/sts/model/AssumeRoleWithWebIdentityRequest.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/common/time_provider/src/time_provider.h"
#include "cpio/client_providers/role_credentials_provider/src/aws/sts_error_converter.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "cpio/common/src/aws/error_codes.h"
#include "google/protobuf/util/time_util.h"

#include "error_codes.h"

using Aws::String;
using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::STS::STSClient;
using Aws::STS::Model::AssumeRoleOutcome;
using Aws::STS::Model::AssumeRoleRequest;
using Aws::STS::Model::AssumeRoleWithWebIdentityOutcome;
using Aws::STS::Model::AssumeRoleWithWebIdentityRequest;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::
    SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::errors::
    SC_AWS_ROLE_CREDENTIALS_PROVIDER_INVALID_REQUEST;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;
using std::chrono::seconds;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace {
constexpr char kAwsRoleCredentialsProvider[] = "AwsRoleCredentialsProvider";
constexpr char kGcpTokenTypeForAws[] = "LIMITED_AWS";
constexpr char kGcpTokenTypeForAwsForKeyIds[] = "AWS_PRINCIPALTAGS";
// Refetch credentials kCredentialsEarlyExpirationIntervalInSeconds before they
// expire.
constexpr int16_t kCredentialsEarlyExpirationIntervalInSeconds = 300;

string CreateRoleCredentialsCacheKey(
    const string& account_identity, const string& target_audience,
    const shared_ptr<vector<string>>& key_ids = nullptr) {
  if (target_audience.empty()) {
    return account_identity;
  }
  if (!key_ids || key_ids->empty()) {
    return absl::StrCat(account_identity, ":", target_audience);
  }
  vector<string> sorted_key_ids = *key_ids;
  std::sort(sorted_key_ids.begin(), sorted_key_ids.end());
  return absl::StrCat(account_identity, ":", target_audience, ":",
                      absl::StrJoin(sorted_key_ids, ","));
}

bool RoleCredentialsAreExpired(
    const google::scp::cpio::client_providers::GetRoleCredentialsResponse&
        credentials_response) {
  return credentials_response.expire_time.count() <
         TimeUtil::GetCurrentTime().seconds() +
             kCredentialsEarlyExpirationIntervalInSeconds;
}
}  // namespace

namespace google::scp::cpio::client_providers {
shared_ptr<ClientConfiguration>
AwsRoleCredentialsProvider::CreateClientConfiguration(
    const string& region) noexcept {
  return common::CreateClientConfiguration(
      make_shared<string>(std::move(region)));
}

ExecutionResult AwsRoleCredentialsProvider::Init() noexcept {
  if (options_->region.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED);
    SCP_ERROR(kAwsRoleCredentialsProvider, kZeroUuid, execution_result,
              "Region option cannot be empty.");
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

  auto client_config = CreateClientConfiguration(options_->region);
  client_config->executor = make_shared<AwsAsyncExecutor>(io_async_executor_);
  sts_client_ = make_shared<STSClient>(*client_config);

  auto timestamp =
      to_string(TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks());
  session_name_ = make_shared<string>(timestamp);

  return SuccessExecutionResult();
}

ExecutionResult AwsRoleCredentialsProvider::Run() noexcept {
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

  if (options_->enable_role_credentials_cache) {
    GetRoleCredentialsResponse cached_response;
    auto cache_key = CreateRoleCredentialsCacheKey(
        *get_credentials_context.request->account_identity,
        get_credentials_context.request->target_audience_for_web_identity,
        get_credentials_context.request->key_ids);
    auto result = cached_role_credentials_.Find(cache_key, cached_response);
    if (result.Successful() && !RoleCredentialsAreExpired(cached_response)) {
      SCP_DEBUG_CONTEXT(
          kAwsRoleCredentialsProvider, get_credentials_context,
          "Found role credentials cache with expiration time %lld.",
          cached_response.expire_time.count());
      get_credentials_context.response =
          make_shared<GetRoleCredentialsResponse>(cached_response);
      get_credentials_context.result = SuccessExecutionResult();
      get_credentials_context.Finish();
      return;
    }
  }

  GetRoleCredentialsInternal(get_credentials_context);
}

void AwsRoleCredentialsProvider::GetRoleCredentialsInternal(
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context) noexcept {
  if (!get_credentials_context.request->target_audience_for_web_identity
           .empty()) {
    auto get_token_request = make_shared<GetTeeSessionTokenRequest>();
    get_token_request->token_type = make_shared<string>(kGcpTokenTypeForAws);
    get_token_request->token_target_audience_uri = make_shared<string>(
        get_credentials_context.request->target_audience_for_web_identity);
    const auto& key_ids = get_credentials_context.request->key_ids;
    if (key_ids) {
      get_token_request->token_type =
          make_shared<string>(kGcpTokenTypeForAwsForKeyIds);
      get_token_request->key_ids = std::move(key_ids);
    }
    AsyncContext<GetTeeSessionTokenRequest, GetSessionTokenResponse>
        get_token_context(
            std::move(get_token_request),
            std::bind(&AwsRoleCredentialsProvider::OnGetTokenCallback, this,
                      get_credentials_context, _1),
            get_credentials_context);

    auth_token_provider_->GetTeeSessionToken(get_token_context);
  } else {
    AssumeRoleRequest sts_request;
    sts_request.SetRoleArn(
        *(get_credentials_context.request->account_identity));
    sts_request.SetRoleSessionName(*session_name_);

    sts_client_->AssumeRoleAsync(
        sts_request,
        std::bind(&AwsRoleCredentialsProvider::OnGetRoleCredentialsCallback,
                  this, get_credentials_context, _1, _2, _3, _4),
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
      std::bind(&AwsRoleCredentialsProvider::
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
    auto execution_result =
        STSErrorConverter::ConvertSTSError(get_credentials_outcome.GetError());

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

  if (options_->enable_role_credentials_cache) {
    const auto& credentials =
        get_credentials_outcome.GetResult().GetCredentials();
    int64_t expiration_seconds = credentials.GetExpiration().Millis() / 1000;
    if (expiration_seconds == 0) {
      auto execution_result =
          FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS);
      SCP_ERROR_CONTEXT(
          kAwsRoleCredentialsProvider, get_credentials_context,
          execution_result,
          "Role credentials expiration time is missing or invalid.");
      get_credentials_context.result = execution_result;
      get_credentials_context.Finish();
      return;
    }
    get_credentials_context.response->expire_time = seconds(expiration_seconds);

    auto cache_key = CreateRoleCredentialsCacheKey(
        *get_credentials_context.request->account_identity,
        get_credentials_context.request->target_audience_for_web_identity,
        get_credentials_context.request->key_ids);
    auto cached_pair =
        std::make_pair(cache_key, *get_credentials_context.response);
    auto erase_result = cached_role_credentials_.Erase(cache_key);
    if (!erase_result.Successful()) {
      SCP_DEBUG_CONTEXT(
          kAwsRoleCredentialsProvider, get_credentials_context,
          "Failed to erase cached role credentials. Cache key is: %s",
          cache_key.c_str());
    }
    auto insert_result = cached_role_credentials_.Insert(
        cached_pair, *get_credentials_context.response);
    if (!insert_result.Successful()) {
      SCP_DEBUG_CONTEXT(
          kAwsRoleCredentialsProvider, get_credentials_context,
          "Failed to insert cached role credentials. Cache key is: %s",
          cache_key.c_str());
    }
  }

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
    auto execution_result =
        STSErrorConverter::ConvertSTSError(get_credentials_outcome.GetError());

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

  if (options_->enable_role_credentials_cache) {
    const auto& credentials =
        get_credentials_outcome.GetResult().GetCredentials();
    int64_t expiration_seconds = credentials.GetExpiration().Millis() / 1000;
    if (expiration_seconds == 0) {
      auto execution_result =
          FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS);
      SCP_ERROR_CONTEXT(
          kAwsRoleCredentialsProvider, get_credentials_context,
          execution_result,
          "Role credentials expiration time is missing or invalid.");
      get_credentials_context.result = execution_result;
      get_credentials_context.Finish();
      return;
    }
    get_credentials_context.response->expire_time = seconds(expiration_seconds);

    auto cache_key = CreateRoleCredentialsCacheKey(
        *get_credentials_context.request->account_identity,
        get_credentials_context.request->target_audience_for_web_identity,
        get_credentials_context.request->key_ids);
    auto cached_pair =
        std::make_pair(cache_key, *get_credentials_context.response);
    auto erase_result = cached_role_credentials_.Erase(cache_key);
    if (!erase_result.Successful()) {
      SCP_DEBUG_CONTEXT(
          kAwsRoleCredentialsProvider, get_credentials_context,
          "Failed to erase cached role credentials. Cache key is: %s",
          cache_key.c_str());
    }
    auto insert_result = cached_role_credentials_.Insert(
        cached_pair, *get_credentials_context.response);
    if (!insert_result.Successful()) {
      SCP_DEBUG_CONTEXT(
          kAwsRoleCredentialsProvider, get_credentials_context,
          "Failed to insert cached role credentials. Cache key is: %s",
          cache_key.c_str());
    }
  }

  get_credentials_context.Finish();
}
}  // namespace google::scp::cpio::client_providers
