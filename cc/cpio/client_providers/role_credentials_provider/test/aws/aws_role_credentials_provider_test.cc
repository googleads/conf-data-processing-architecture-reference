// Copyright 2022 Google LLC
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

#include <string>

#include <aws/core/Aws.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/STSErrors.h>
#include <aws/sts/model/AssumeRoleRequest.h>
#include <aws/sts/model/AssumeRoleWithWebIdentityRequest.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_executor_interface.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/scp_test_base.h"
#include "cpio/client_providers/auth_token_provider/mock/mock_auth_token_provider.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/role_credentials_provider/mock/aws/mock_aws_role_credentials_provider_with_overrides.h"
#include "cpio/client_providers/role_credentials_provider/mock/aws/mock_aws_sts_client.h"
#include "cpio/client_providers/role_credentials_provider/src/aws/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Client::AsyncCallerContext;
using Aws::Client::AWSError;
using Aws::STS::AssumeRoleResponseReceivedHandler;
using Aws::STS::AssumeRoleWithWebIdentityResponseReceivedHandler;
using Aws::STS::STSClient;
using Aws::STS::STSErrors;
using Aws::STS::Model::AssumeRoleOutcome;
using Aws::STS::Model::AssumeRoleRequest;
using Aws::STS::Model::AssumeRoleResult;
using Aws::STS::Model::AssumeRoleWithWebIdentityOutcome;
using Aws::STS::Model::AssumeRoleWithWebIdentityRequest;
using Aws::STS::Model::AssumeRoleWithWebIdentityResult;
using Aws::STS::Model::Credentials;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::
    SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::errors::
    SC_AWS_ROLE_CREDENTIALS_PROVIDER_INVALID_REQUEST;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockAuthTokenProvider;
using google::scp::cpio::client_providers::mock::
    MockAwsRoleCredentialsProviderWithOverrides;
using google::scp::cpio::client_providers::mock::MockSTSClient;
using std::atomic;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kAssumeRoleArn[] = "assume_role_arn";
constexpr char kSessionName[] = "session_name";
constexpr char kKeyId[] = "key_id";
constexpr char kAccessKey[] = "access_key";
constexpr char kTeeSessionToken[] = "tee_session_token";
constexpr char kSecurityToken[] = "session_token";
constexpr char kAudience[] = "www.google.com";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class AwsRoleCredentialsProviderTest : public ScpTestBase {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  void SetUp() override {
    role_credentials_provider_ =
        make_shared<MockAwsRoleCredentialsProviderWithOverrides>(
            make_shared<RoleCredentialsProviderOptions>());
    EXPECT_SUCCESS(role_credentials_provider_->Init());
    role_credentials_provider_->GetInstanceClientProvider()
        ->instance_resource_name = kResourceNameMock;
    EXPECT_SUCCESS(role_credentials_provider_->Run());
    mock_sts_client_ = role_credentials_provider_->GetSTSClient();
    mock_auth_token_provider_ =
        role_credentials_provider_->GetAuthTokenProvider();
  }

  void TearDown() override {
    EXPECT_SUCCESS(role_credentials_provider_->Stop());
  }

  shared_ptr<MockAwsRoleCredentialsProviderWithOverrides>
      role_credentials_provider_;
  shared_ptr<MockSTSClient> mock_sts_client_;
  shared_ptr<MockAuthTokenProvider> mock_auth_token_provider_;
};

TEST_F(AwsRoleCredentialsProviderTest, AssumRoleSuccess) {
  EXPECT_CALL(*mock_sts_client_, AssumeRoleAsync)
      .WillOnce([&](const AssumeRoleRequest& request,
                    const AssumeRoleResponseReceivedHandler& handler,
                    const shared_ptr<const AsyncCallerContext>& context) {
        EXPECT_EQ(request.GetRoleArn(), kAssumeRoleArn);
        EXPECT_EQ(request.GetRoleSessionName(), kSessionName);

        AssumeRoleResult result;
        Credentials credentials;
        credentials.SetAccessKeyId(kKeyId);
        credentials.SetSecretAccessKey(kAccessKey);
        credentials.SetSessionToken(kSecurityToken);
        result.SetCredentials(credentials);
        AssumeRoleOutcome outcome(result);
        handler(mock_sts_client_.get(), request, outcome, context);
      });

  atomic<bool> finished = false;
  auto request = make_shared<GetRoleCredentialsRequest>();
  request->account_identity = make_shared<string>(kAssumeRoleArn);
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_credentials_context(
          move(request),
          [&](AsyncContext<GetRoleCredentialsRequest,
                           GetRoleCredentialsResponse>& context) {
            EXPECT_SUCCESS(context.result);

            EXPECT_EQ(*context.response->access_key_id, kKeyId);
            EXPECT_EQ(*context.response->access_key_secret, kAccessKey);
            EXPECT_EQ(*context.response->security_token, kSecurityToken);

            finished = true;
          });
  role_credentials_provider_->GetRoleCredentials(get_credentials_context);

  WaitUntil([&]() { return finished.load(); });
}

TEST_F(AwsRoleCredentialsProviderTest, AssumRoleFailure) {
  EXPECT_CALL(*mock_sts_client_, AssumeRoleAsync)
      .WillOnce([&](const AssumeRoleRequest& request,
                    const AssumeRoleResponseReceivedHandler& handler,
                    const shared_ptr<const AsyncCallerContext>& context) {
        EXPECT_EQ(request.GetRoleArn(), kAssumeRoleArn);
        EXPECT_EQ(request.GetRoleSessionName(), kSessionName);

        AWSError<STSErrors> sts_error(STSErrors::INVALID_ACTION, false);
        AssumeRoleOutcome outcome(sts_error);
        handler(mock_sts_client_.get(), request, outcome, context);
      });

  auto is_called = false;
  auto request = make_shared<GetRoleCredentialsRequest>();
  request->account_identity = make_shared<string>(kAssumeRoleArn);
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_credentials_context(
          move(request),
          [&](AsyncContext<GetRoleCredentialsRequest,
                           GetRoleCredentialsResponse>& context) {
            EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(
                                            SC_AWS_INTERNAL_SERVICE_ERROR)));
            is_called = true;
          });

  role_credentials_provider_->GetRoleCredentials(get_credentials_context);

  EXPECT_EQ(is_called, true);
}

TEST_F(AwsRoleCredentialsProviderTest,
       AssumRoleFailureDueToMissingAccountIdentity) {
  EXPECT_CALL(*mock_sts_client_, AssumeRoleAsync).Times(0);

  auto is_called = false;
  auto request = make_shared<GetRoleCredentialsRequest>();
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_credentials_context(
          move(request),
          [&](AsyncContext<GetRoleCredentialsRequest,
                           GetRoleCredentialsResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(
                            SC_AWS_ROLE_CREDENTIALS_PROVIDER_INVALID_REQUEST)));
            is_called = true;
          });

  role_credentials_provider_->GetRoleCredentials(get_credentials_context);

  EXPECT_EQ(is_called, true);
}

TEST_F(AwsRoleCredentialsProviderTest, AssumRoleWithWebIdentitySuccess) {
  EXPECT_CALL(*mock_auth_token_provider_, GetTeeSessionToken)
      .WillOnce([](AsyncContext<GetTeeSessionTokenRequest,
                                GetSessionTokenResponse>& context) {
        EXPECT_EQ(*context.request->token_target_audience_uri, kAudience);
        EXPECT_EQ(*context.request->token_type, "LIMITED_AWS");

        context.result = SuccessExecutionResult();
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token = make_shared<string>(kTeeSessionToken);
        context.Finish();
      });

  EXPECT_CALL(*mock_sts_client_, AssumeRoleWithWebIdentityAsync)
      .WillOnce(
          [&](const AssumeRoleWithWebIdentityRequest& request,
              const AssumeRoleWithWebIdentityResponseReceivedHandler& handler,
              const shared_ptr<const AsyncCallerContext>& context) {
            EXPECT_EQ(request.GetRoleArn(), kAssumeRoleArn);
            EXPECT_EQ(request.GetRoleSessionName(), kSessionName);
            EXPECT_EQ(request.GetWebIdentityToken(), kTeeSessionToken);

            AssumeRoleWithWebIdentityResult result;
            Credentials credentials;
            credentials.SetAccessKeyId(kKeyId);
            credentials.SetSecretAccessKey(kAccessKey);
            credentials.SetSessionToken(kSecurityToken);
            result.SetCredentials(credentials);
            AssumeRoleWithWebIdentityOutcome outcome(result);
            handler(mock_sts_client_.get(), request, outcome, context);
          });

  atomic<bool> finished = false;
  auto request = make_shared<GetRoleCredentialsRequest>();
  request->account_identity = make_shared<string>(kAssumeRoleArn);
  request->target_audience_for_web_identity = kAudience;
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_credentials_context(
          move(request),
          [&](AsyncContext<GetRoleCredentialsRequest,
                           GetRoleCredentialsResponse>& context) {
            EXPECT_SUCCESS(context.result);

            EXPECT_EQ(*context.response->access_key_id, kKeyId);
            EXPECT_EQ(*context.response->access_key_secret, kAccessKey);
            EXPECT_EQ(*context.response->security_token, kSecurityToken);

            finished = true;
          });
  role_credentials_provider_->GetRoleCredentials(get_credentials_context);

  WaitUntil([&]() { return finished.load(); });
}

TEST_F(AwsRoleCredentialsProviderTest, GetTeeTokenFailure) {
  EXPECT_CALL(*mock_auth_token_provider_, GetTeeSessionToken)
      .WillOnce([](AsyncContext<GetTeeSessionTokenRequest,
                                GetSessionTokenResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  EXPECT_CALL(*mock_sts_client_, AssumeRoleWithWebIdentityAsync).Times(0);

  atomic<bool> finished = false;
  auto request = make_shared<GetRoleCredentialsRequest>();
  request->account_identity = make_shared<string>(kAssumeRoleArn);
  request->target_audience_for_web_identity = kAudience;
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_credentials_context(
          move(request),
          [&](AsyncContext<GetRoleCredentialsRequest,
                           GetRoleCredentialsResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished = true;
          });
  role_credentials_provider_->GetRoleCredentials(get_credentials_context);

  WaitUntil([&]() { return finished.load(); });
}

TEST_F(AwsRoleCredentialsProviderTest, AssumRoleWithWebIdentityFailure) {
  EXPECT_CALL(*mock_auth_token_provider_, GetTeeSessionToken)
      .WillOnce([](AsyncContext<GetTeeSessionTokenRequest,
                                GetSessionTokenResponse>& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token = make_shared<string>(kTeeSessionToken);
        context.Finish();
      });

  EXPECT_CALL(*mock_sts_client_, AssumeRoleWithWebIdentityAsync)
      .WillOnce(
          [&](const AssumeRoleWithWebIdentityRequest& request,
              const AssumeRoleWithWebIdentityResponseReceivedHandler& handler,
              const shared_ptr<const AsyncCallerContext>& context) {
            EXPECT_EQ(request.GetRoleArn(), kAssumeRoleArn);
            EXPECT_EQ(request.GetRoleSessionName(), kSessionName);
            EXPECT_EQ(request.GetWebIdentityToken(), kTeeSessionToken);

            AWSError<STSErrors> sts_error(STSErrors::INVALID_ACTION, false);
            AssumeRoleWithWebIdentityOutcome outcome(sts_error);
            handler(mock_sts_client_.get(), request, outcome, context);
          });

  atomic<bool> finished = false;
  auto request = make_shared<GetRoleCredentialsRequest>();
  request->account_identity = make_shared<string>(kAssumeRoleArn);
  request->target_audience_for_web_identity = kAudience;
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_credentials_context(
          move(request),
          [&](AsyncContext<GetRoleCredentialsRequest,
                           GetRoleCredentialsResponse>& context) {
            EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(
                                            SC_AWS_INTERNAL_SERVICE_ERROR)));

            finished = true;
          });
  role_credentials_provider_->GetRoleCredentials(get_credentials_context);

  WaitUntil([&]() { return finished.load(); });
}

TEST_F(AwsRoleCredentialsProviderTest,
       NullInstanceClientProviderAndEmptyRegion) {
  auto role_credentials_provider = make_shared<AwsRoleCredentialsProvider>(
      make_shared<RoleCredentialsProviderOptions>(), nullptr,
      make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
      make_shared<MockAuthTokenProvider>());
  EXPECT_SUCCESS(role_credentials_provider->Init());
  EXPECT_THAT(role_credentials_provider->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED)));
}

TEST_F(AwsRoleCredentialsProviderTest, InputRegion) {
  auto options = make_shared<RoleCredentialsProviderOptions>();
  options->region = "us-east-1";
  auto role_credentials_provider = make_shared<AwsRoleCredentialsProvider>(
      std::move(options), nullptr, make_shared<MockAsyncExecutor>(),
      make_shared<MockAsyncExecutor>(), make_shared<MockAuthTokenProvider>());
  EXPECT_SUCCESS(role_credentials_provider->Init());
  EXPECT_SUCCESS(role_credentials_provider->Run());
  EXPECT_SUCCESS(role_credentials_provider->Stop());
}

TEST_F(AwsRoleCredentialsProviderTest, NullCpuAsyncExecutor) {
  auto role_credentials_provider = make_shared<AwsRoleCredentialsProvider>(
      make_shared<RoleCredentialsProviderOptions>(),
      make_shared<mock::MockInstanceClientProvider>(), nullptr,
      make_shared<MockAsyncExecutor>(), make_shared<MockAuthTokenProvider>());
  EXPECT_SUCCESS(role_credentials_provider->Init());
  EXPECT_THAT(role_credentials_provider->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED)));
}

TEST_F(AwsRoleCredentialsProviderTest, NullIoAsyncExecutor) {
  auto role_credentials_provider = make_shared<AwsRoleCredentialsProvider>(
      make_shared<RoleCredentialsProviderOptions>(),
      make_shared<mock::MockInstanceClientProvider>(),
      make_shared<MockAsyncExecutor>(), nullptr,
      make_shared<MockAuthTokenProvider>());
  EXPECT_SUCCESS(role_credentials_provider->Init());
  EXPECT_THAT(role_credentials_provider->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED)));
}

TEST_F(AwsRoleCredentialsProviderTest, NullAuthTokenProvider) {
  auto role_credentials_provider = make_shared<AwsRoleCredentialsProvider>(
      make_shared<RoleCredentialsProviderOptions>(),
      make_shared<mock::MockInstanceClientProvider>(),
      make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
      nullptr);
  EXPECT_SUCCESS(role_credentials_provider->Init());
  EXPECT_THAT(role_credentials_provider->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED)));
}
}  // namespace google::scp::cpio::client_providers::test
