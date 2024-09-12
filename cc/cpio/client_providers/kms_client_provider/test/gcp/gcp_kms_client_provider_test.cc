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

#include "cpio/client_providers/kms_client_provider/src/gcp/gcp_kms_client_provider.h"

#include <gmock/gmock.h>

#include <memory>
#include <string>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/scp_test_base.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/kms_client_provider/mock/gcp/mock_gcp_key_management_service_client.h"
#include "cpio/client_providers/kms_client_provider/src/gcp/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::cloud::Status;
using google::cloud::StatusCode;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::test::ScpTestBase;
using GcsDecryptRequest = google::cloud::kms::v1::DecryptRequest;
using GcsDecryptResponse = google::cloud::kms::v1::DecryptResponse;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::mock::
    MockGcpKeyManagementServiceClient;
using std::atomic;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::Return;

static constexpr char kServiceAccount[] = "account";
static constexpr char kWipProvider[] = "wip";
static constexpr char kKeyArn[] = "keyArn";
static constexpr char kCiphertext[] = "ciphertext";
static constexpr char kPlaintext[] = "plaintext";

namespace google::scp::cpio::client_providers::test {
class MockGcpKmsFactory : public GcpKmsFactory {
 public:
  MOCK_METHOD(shared_ptr<GcpKeyManagementServiceClientInterface>,
              CreateGcpKeyManagementServiceClient,
              (const string&, const string&), (noexcept, override));
};

class GcpKmsClientProviderTest : public ScpTestBase {
 protected:
  void SetUp() override {
    mock_gcp_kms_factory_ = make_shared<MockGcpKmsFactory>();
    client_ = make_unique<GcpKmsClientProvider>(mock_io_async_executor_,
                                                mock_cpu_async_executor_,
                                                mock_gcp_kms_factory_);
    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());

    mock_gcp_key_management_service_client_ =
        std::make_shared<MockGcpKeyManagementServiceClient>();
  }

  void TearDown() override { EXPECT_SUCCESS(client_->Stop()); }

  unique_ptr<GcpKmsClientProvider> client_;
  shared_ptr<MockGcpKmsFactory> mock_gcp_kms_factory_;
  shared_ptr<MockGcpKeyManagementServiceClient>
      mock_gcp_key_management_service_client_;
  shared_ptr<MockAsyncExecutor> mock_io_async_executor_ =
      make_shared<MockAsyncExecutor>();
  shared_ptr<MockAsyncExecutor> mock_cpu_async_executor_ =
      make_shared<MockAsyncExecutor>();
};

TEST_F(GcpKmsClientProviderTest, NullKeyArn) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND)));
        condition = true;
      });

  client_->Decrypt(context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpKmsClientProviderTest, EmptyKeyArn) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name("");
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND)));
        condition = true;
      });

  client_->Decrypt(context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpKmsClientProviderTest, NullCiphertext) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND)));
        condition = true;
      });

  client_->Decrypt(context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpKmsClientProviderTest, EmptyCiphertext) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext("");
  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND)));
        condition = true;
      });

  client_->Decrypt(context);
  WaitUntil([&]() { return condition.load(); });
}

MATCHER_P(RequestMatches, req, "") {
  return ExplainMatchResult(Eq(req.name()), arg.name(), result_listener) &&
         ExplainMatchResult(Eq(req.ciphertext()), arg.ciphertext(),
                            result_listener) &&
         ExplainMatchResult(Eq(req.additional_authenticated_data()),
                            arg.additional_authenticated_data(),
                            result_listener);
}

TEST_F(GcpKmsClientProviderTest, FailedToDecode) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext("abc");
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED)));
        condition = true;
      });

  client_->Decrypt(context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpKmsClientProviderTest, SuccessToDecrypt) {
  EXPECT_CALL(*mock_gcp_kms_factory_, CreateGcpKeyManagementServiceClient(
                                          kWipProvider, kServiceAccount))
      .WillOnce(Return(mock_gcp_key_management_service_client_));

  ASSERT_SUCCESS_AND_ASSIGN(string encoded_ciphertext,
                            Base64Encode(kCiphertext));

  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext(encoded_ciphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  GcsDecryptRequest decrypt_request;
  decrypt_request.set_name(kKeyArn);
  decrypt_request.set_ciphertext(kCiphertext);
  GcsDecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  EXPECT_CALL(*mock_gcp_key_management_service_client_,
              Decrypt(RequestMatches(decrypt_request)))
      .WillOnce(Return(decrypt_response));

  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->plaintext(), kPlaintext);
        condition = true;
      });

  client_->Decrypt(context);

  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpKmsClientProviderTest, FailedToDecrypt) {
  EXPECT_CALL(*mock_gcp_kms_factory_, CreateGcpKeyManagementServiceClient(
                                          kWipProvider, kServiceAccount))
      .WillOnce(Return(mock_gcp_key_management_service_client_));

  string encoded_ciphertext = *Base64Encode(kCiphertext);
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext(encoded_ciphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  GcsDecryptRequest decrypt_request;
  decrypt_request.set_name(kKeyArn);
  decrypt_request.set_ciphertext(kCiphertext);
  GcsDecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  EXPECT_CALL(*mock_gcp_key_management_service_client_,
              Decrypt(RequestMatches(decrypt_request)))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "Invalid input")));

  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED)));
        condition = true;
      });

  client_->Decrypt(context);

  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
