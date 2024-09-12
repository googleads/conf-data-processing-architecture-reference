// Copyright 2024 Google LLC
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

#include "public/cpio/adapters/kms_client/src/aws/aws_kms_client.h"

#include <memory>

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "cpio/client_providers/auth_token_provider/mock/mock_auth_token_provider.h"
#include "cpio/client_providers/kms_client_provider/mock/mock_kms_client_provider.h"
#include "cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/kms_client/src/kms_client.h"
#include "public/cpio/interface/kms_client/kms_client_interface.h"
#include "public/cpio/test/global_cpio/test_cpio_options.h"
#include "public/cpio/test/global_cpio/test_lib_cpio.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::ScpTestBase;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::KmsClientProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderOptions;
using google::scp::cpio::client_providers::mock::MockAuthTokenProvider;
using google::scp::cpio::client_providers::mock::MockKmsClientProvider;
using google::scp::cpio::client_providers::mock::MockRoleCredentialsProvider;
using std::atomic_bool;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using ::testing::_;
using ::testing::Return;

namespace google::scp::cpio::test {
class MockAwsRoleCredentialsProviderFactory
    : public AwsRoleCredentialsProviderFactory {
 public:
  MOCK_METHOD(shared_ptr<RoleCredentialsProviderInterface>, Create,
              (const string&,
               const shared_ptr<InstanceClientProviderInterface>&,
               const shared_ptr<AsyncExecutorInterface>&,
               const shared_ptr<AsyncExecutorInterface>&,
               const shared_ptr<AuthTokenProviderInterface>&),
              (noexcept, override));
};

class MockAwsKmsClientProviderFactory : public AwsKmsClientProviderFactory {
 public:
  MOCK_METHOD(shared_ptr<KmsClientProviderInterface>, Create,
              (const shared_ptr<AwsKmsClientOptions>&,
               const shared_ptr<RoleCredentialsProviderInterface>&,
               const shared_ptr<AsyncExecutorInterface>&,
               const shared_ptr<AsyncExecutorInterface>&),
              (noexcept, override));
};

class AwsKmsClientTest : public ScpTestBase {
 protected:
  AwsKmsClientTest() {
    TestCpioOptions options;
    options.log_option = LogOption::kNoLog;
    EXPECT_SUCCESS(TestLibCpio::InitCpio(options));

    mock_role_credentials_provider_factory_ =
        make_shared<MockAwsRoleCredentialsProviderFactory>();
    mock_kms_client_provider_factory_ =
        make_shared<MockAwsKmsClientProviderFactory>();
    mock_role_credentials_provider_ =
        make_shared<MockRoleCredentialsProvider>();
    mock_kms_client_provider_ = make_shared<MockKmsClientProvider>();
    mock_auth_token_provider_ = make_shared<MockAuthTokenProvider>();
  }

  ~AwsKmsClientTest() {
    TestCpioOptions options;
    options.log_option = LogOption::kNoLog;
    EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(options));
  }

  unique_ptr<AwsKmsClient> client_;
  shared_ptr<MockAwsRoleCredentialsProviderFactory>
      mock_role_credentials_provider_factory_;
  shared_ptr<MockAwsKmsClientProviderFactory> mock_kms_client_provider_factory_;
  shared_ptr<MockRoleCredentialsProvider> mock_role_credentials_provider_;
  shared_ptr<MockKmsClientProvider> mock_kms_client_provider_;
  shared_ptr<MockAuthTokenProvider> mock_auth_token_provider_;
};

MATCHER_P(InstanceClientProviderIsNull, instance_client_provider_is_null, "") {
  if (instance_client_provider_is_null) {
    return arg == nullptr;
  } else {
    return arg != nullptr;
  }
}

MATCHER_P(RegionMatch, region, "") {
  return arg == region;
}

TEST_F(AwsKmsClientTest, CreateSuccessfullyWithoutRegion) {
  client_ = make_unique<AwsKmsClient>(make_shared<AwsKmsClientOptions>(),
                                      mock_role_credentials_provider_factory_,
                                      mock_kms_client_provider_factory_);

  EXPECT_CALL(
      *mock_role_credentials_provider_factory_,
      Create(RegionMatch(""), InstanceClientProviderIsNull(false), _, _, _))
      .WillOnce(Return(mock_role_credentials_provider_));
  EXPECT_CALL(*mock_kms_client_provider_factory_, Create)
      .WillOnce(Return(mock_kms_client_provider_));

  EXPECT_CALL(*mock_role_credentials_provider_, Init)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_kms_client_provider_, Init)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_role_credentials_provider_, Run)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_kms_client_provider_, Run)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_role_credentials_provider_, Stop)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_kms_client_provider_, Stop)
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());
  EXPECT_SUCCESS(client_->Stop());
}

TEST_F(AwsKmsClientTest, CreateSuccessfullyWithRegion) {
  auto options = make_shared<AwsKmsClientOptions>();
  options->region = "us-east-1";
  client_ = make_unique<AwsKmsClient>(std::move(options),
                                      mock_role_credentials_provider_factory_,
                                      mock_kms_client_provider_factory_);

  EXPECT_CALL(*mock_role_credentials_provider_factory_,
              Create(RegionMatch(options->region),
                     InstanceClientProviderIsNull(true), _, _, _))
      .WillOnce(Return(mock_role_credentials_provider_));
  EXPECT_CALL(*mock_kms_client_provider_factory_, Create)
      .WillOnce(Return(mock_kms_client_provider_));

  EXPECT_CALL(*mock_role_credentials_provider_, Init)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_kms_client_provider_, Init)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_role_credentials_provider_, Run)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_kms_client_provider_, Run)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_role_credentials_provider_, Stop)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_CALL(*mock_kms_client_provider_, Stop)
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());
  EXPECT_SUCCESS(client_->Stop());
}
}  // namespace google::scp::cpio::test
