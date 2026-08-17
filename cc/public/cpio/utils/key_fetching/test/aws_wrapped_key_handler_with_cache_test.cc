// Copyright 2026 Google LLC
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

#include "public/cpio/utils/key_fetching/src/aws_wrapped_key_handler_with_cache.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "cc/core/test/utils/scp_test_base.h"
#include "cpio/common/src/common_error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/error_codes.h"
#include "public/cpio/mock/kms_client/mock_kms_client.h"
#include "public/cpio/utils/dual_writing_metric_client/mock/dual_writing_metric_client_mock.h"
#include "public/cpio/utils/key_fetching/src/error_codes.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"
#include "public/cpio/utils/key_fetching/test/key_fetching_metric_matcher.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::cmrt::sdk::v1::AwsWrappedKey;
using google::cmrt::sdk::v1::CloudWrappedKey;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_CPIO_INTERNAL_ERROR;
using google::scp::core::errors::SC_CPIO_INVALID_ARGUMENT;
using google::scp::core::errors::SC_CPIO_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_CPIO_KEY_FETCHER_FETCHING_TIMEOUT;
using google::scp::core::errors::SC_CPIO_KEY_NOT_FOUND;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::cpio::DualWritingMetricClientMock;
using google::scp::cpio::ExpectOtelKeyCacheStatusMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingErrorMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingLatencyMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingRequestMetricPush;
using google::scp::cpio::KeyCacheStatus;
using google::scp::cpio::KeyFetchingType;
using google::scp::cpio::KeyType;
using google::scp::cpio::MockKmsClient;
using google::scp::cpio::WrappedKeyHandlerOptions;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::chrono::seconds;
using std::this_thread::sleep_for;
using ::testing::Eq;
using testing::NiceMock;
using testing::Return;

namespace google::scp::cpio {

namespace {

constexpr absl::string_view kEncryptedDek = "encrypted_dek";
constexpr absl::string_view kAwsKmsResourceName =
    "arn:aws:kms:us-east-1:123456789012:key/test-key";
constexpr absl::string_view kAwsKmsRegion = "us-east-1";
constexpr absl::string_view kRoleArn =
    "arn:aws:iam::123456789012:role/test-role";
constexpr absl::string_view kContainerImageSignatureKeyId =
    "aws_kms_default_signatures";
constexpr absl::string_view kAudience = "aws_kms_default_audience";

CloudWrappedKey BuildCloudWrappedKey(const AwsWrappedKey& aws_wrapped_key) {
  CloudWrappedKey cloud_wrapped_key;
  *cloud_wrapped_key.mutable_aws_wrapped_key() = aws_wrapped_key;
  return cloud_wrapped_key;
}

WrappedKeyHandlerOptions CreateWrappedKeyHandlerOptions(
    bool enable_decryption_lock = true, bool enable_cache = true,
    const std::vector<std::string>& image_signature_key_ids = {std::string(
        kContainerImageSignatureKeyId)},
    const std::string& aws_target_audience_for_web_identity =
        std::string(kAudience)) {
  WrappedKeyHandlerOptions options;
  options.enable_decryption_lock = enable_decryption_lock;
  options.enable_cache = enable_cache;
  options.key_cache_lifetime = std::chrono::seconds(1800);
  options.key_failure_cache_lifetime = std::chrono::seconds(300);
  options.key_decryption_waiting_timeout = std::chrono::milliseconds(200);
  options.image_signature_key_ids = image_signature_key_ids;
  options.aws_target_audience_for_web_identity =
      aws_target_audience_for_web_identity;
  return options;
}

}  // namespace

class AwsWrappedKeyHandlerWithCacheTest : public ScpTestBase {
 public:
  AwsWrappedKeyHandlerWithCacheTest()
      : async_executor_(make_shared<AsyncExecutor>(2, 10)),
        wrapped_key_handler_(async_executor_, mock_kms_client_,
                             CreateWrappedKeyHandlerOptions(),
                             mock_metric_client_) {
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());
    EXPECT_SUCCESS(wrapped_key_handler_.Init());
    EXPECT_SUCCESS(wrapped_key_handler_.Run());
  }

 protected:
  void SetUp() override {
    aws_wrapped_key_.set_encrypted_dek(string(kEncryptedDek));
    aws_wrapped_key_.set_kek_uri(string(kAwsKmsResourceName));
    aws_wrapped_key_.set_role_arn(string(kRoleArn));

    expected_decrypt_request_.set_ciphertext(string(kEncryptedDek));
    expected_decrypt_request_.set_key_resource_name(
        string(kAwsKmsResourceName));
    expected_decrypt_request_.set_kms_region(string(kAwsKmsRegion));
    expected_decrypt_request_.set_account_identity(string(kRoleArn));
    expected_decrypt_request_.add_key_ids(
        string(kContainerImageSignatureKeyId));
    expected_decrypt_request_.set_target_audience_for_web_identity(
        string(kAudience));
  }

  void TearDown() override {
    EXPECT_SUCCESS(wrapped_key_handler_.Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
  }

  void ExpectOtelEncryptionKeyFetchingRequestMetricPush(int call_count) {
    ExpectOtelKeyFetchingRequestMetricPush(
        mock_metric_client_, call_count, KeyType::kAwsWrappedKey,
        KeyFetchingType::kOnDemand, kDummyLabelValue);
  }

  void ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      int call_count,
      absl::string_view error_code = KeyFetchingErrorType::kGenericError) {
    ExpectOtelKeyFetchingErrorMetricPush(
        mock_metric_client_, call_count, KeyType::kAwsWrappedKey,
        KeyFetchingType::kOnDemand, kDummyLabelValue, error_code);
  }

  void ExpectOtelEncryptionKeyFetchingLatencyMetricPush(int call_count) {
    ExpectOtelKeyFetchingLatencyMetricPush(
        mock_metric_client_, call_count, KeyType::kAwsWrappedKey,
        KeyFetchingType::kOnDemand, kDummyLabelValue);
  }

  void ExpectOtelEncryptionKeyCacheStatusMetricPush(
      int call_count,
      absl::string_view key_cache_status = KeyCacheStatus::kValidKeyCacheHit) {
    ExpectOtelKeyCacheStatusMetricPush(mock_metric_client_, call_count,
                                       KeyType::kAwsWrappedKey,
                                       kDummyLabelValue, key_cache_status);
  }

  std::shared_ptr<AsyncExecutorInterface> async_executor_;
  NiceMock<DualWritingMetricClientMock> mock_metric_client_;
  MockKmsClient mock_kms_client_;
  AwsWrappedKeyHandlerWithCache wrapped_key_handler_;
  AwsWrappedKey aws_wrapped_key_;
  DecryptRequest expected_decrypt_request_;
};

TEST_F(AwsWrappedKeyHandlerWithCacheTest, GetKeyFailsIfAwsWrappedKeyIsMissing) {
  CloudWrappedKey cloud_wrapped_key;  // Empty, no aws_wrapped_key

  auto result = wrapped_key_handler_.GetKey(cloud_wrapped_key);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest, GetKeyFailsIfFieldsAreEmpty) {
  aws_wrapped_key_.clear_encrypted_dek();
  EXPECT_THAT(
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(aws_wrapped_key_)),
      ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));

  aws_wrapped_key_.set_encrypted_dek(kEncryptedDek);
  aws_wrapped_key_.clear_kek_uri();
  EXPECT_THAT(
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(aws_wrapped_key_)),
      ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));

  aws_wrapped_key_.set_kek_uri(kAwsKmsResourceName);
  aws_wrapped_key_.clear_role_arn();
  EXPECT_THAT(
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(aws_wrapped_key_)),
      ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       CreateDecryptRequestFailsIfKekUriIsInvalid) {
  aws_wrapped_key_.set_kek_uri("arn:aws:kms");  // Less than 4 parts

  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(0);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingErrorType::kInvalidKeyId);

  EXPECT_CALL(mock_kms_client_, DecryptSync).Times(0);

  auto result =
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(aws_wrapped_key_));
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       CreateDecryptRequestMapsFieldsCorrectly) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  DecryptResponse response;
  response.set_plaintext("decrypted_dek");
  EXPECT_CALL(mock_kms_client_,
              DecryptSync(EqualsProto(expected_decrypt_request_)))
      .WillOnce(Return(response));

  wrapped_key_handler_.GetKey(BuildCloudWrappedKey(aws_wrapped_key_));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       CreateDecryptRequestWithEmptySignatures) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  AwsWrappedKeyHandlerWithCache handler_without_signatures(
      async_executor_, mock_kms_client_,
      CreateWrappedKeyHandlerOptions(
          /*enable_decryption_lock=*/true, /*enable_cache=*/true,
          /*image_signature_key_ids=*/{},
          /*aws_target_audience_for_web_identity=*/""),
      mock_metric_client_);
  EXPECT_SUCCESS(handler_without_signatures.Init());
  EXPECT_SUCCESS(handler_without_signatures.Run());

  DecryptRequest expected_request_without_signatures =
      expected_decrypt_request_;
  expected_request_without_signatures.clear_key_ids();
  expected_request_without_signatures.clear_target_audience_for_web_identity();

  DecryptResponse response;
  response.set_plaintext("decrypted_dek");
  EXPECT_CALL(mock_kms_client_,
              DecryptSync(EqualsProto(expected_request_without_signatures)))
      .WillOnce(Return(response));

  handler_without_signatures.GetKey(BuildCloudWrappedKey(aws_wrapped_key_));
  EXPECT_SUCCESS(handler_without_signatures.Stop());
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       CreateDecryptRequestWithMultipleSignatures) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  std::vector<std::string> signatures = {"sig1", "sig2", "sig3"};
  AwsWrappedKeyHandlerWithCache handler_with_multiple_signatures(
      async_executor_, mock_kms_client_,
      CreateWrappedKeyHandlerOptions(
          /*enable_decryption_lock=*/true, /*enable_cache=*/true,
          /*image_signature_key_ids=*/signatures,
          /*aws_target_audience_for_web_identity=*/string(kAudience)),
      mock_metric_client_);
  EXPECT_SUCCESS(handler_with_multiple_signatures.Init());
  EXPECT_SUCCESS(handler_with_multiple_signatures.Run());

  DecryptRequest expected_request = expected_decrypt_request_;
  expected_request.clear_key_ids();
  for (const auto& sig : signatures) {
    expected_request.add_key_ids(sig);
  }

  DecryptResponse response;
  response.set_plaintext("decrypted_dek");
  EXPECT_CALL(mock_kms_client_, DecryptSync(EqualsProto(expected_request)))
      .WillOnce(Return(response));

  handler_with_multiple_signatures.GetKey(
      BuildCloudWrappedKey(aws_wrapped_key_));
  EXPECT_SUCCESS(handler_with_multiple_signatures.Stop());
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       CreateDecryptRequestWithEmptyAudience) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  AwsWrappedKeyHandlerWithCache handler_without_audience(
      async_executor_, mock_kms_client_,
      CreateWrappedKeyHandlerOptions(
          /*enable_decryption_lock=*/true, /*enable_cache=*/true,
          /*image_signature_key_ids=*/{string(kContainerImageSignatureKeyId)},
          /*aws_target_audience_for_web_identity=*/""),
      mock_metric_client_);
  EXPECT_SUCCESS(handler_without_audience.Init());
  EXPECT_SUCCESS(handler_without_audience.Run());

  DecryptRequest expected_request_without_audience = expected_decrypt_request_;
  expected_request_without_audience.clear_target_audience_for_web_identity();

  DecryptResponse response;
  response.set_plaintext("decrypted_dek");
  EXPECT_CALL(mock_kms_client_,
              DecryptSync(EqualsProto(expected_request_without_audience)))
      .WillOnce(Return(response));

  handler_without_audience.GetKey(BuildCloudWrappedKey(aws_wrapped_key_));
  EXPECT_SUCCESS(handler_without_audience.Stop());
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessful) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(response));

  auto decrypted_dek_or =
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(aws_wrapped_key_));

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeyWithKekTinkPrefixSuccessful) {
  auto kek_uri_with_prefix = absl::StrCat("aws-kms://", kAwsKmsResourceName);
  aws_wrapped_key_.set_kek_uri(kek_uri_with_prefix);

  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_,
              DecryptSync(EqualsProto(expected_decrypt_request_)))
      .WillOnce(Return(response));

  auto decrypted_dek_or =
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(aws_wrapped_key_));

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessfulWithMutexLockEnabled) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  AwsWrappedKeyHandlerWithCache key_handler_with_mutex_lock_enabled(
      async_executor_, mock_kms_client_, CreateWrappedKeyHandlerOptions(true),
      mock_metric_client_);
  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(response));

  auto decrypted_dek_or = key_handler_with_mutex_lock_enabled.GetKey(
      BuildCloudWrappedKey(aws_wrapped_key_));

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessfulWithMutexLockDisabled) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  AwsWrappedKeyHandlerWithCache key_handler_with_mutex_lock_disabled(
      async_executor_, mock_kms_client_, CreateWrappedKeyHandlerOptions(false),
      mock_metric_client_);
  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(response));

  auto decrypted_dek_or = key_handler_with_mutex_lock_disabled.GetKey(
      BuildCloudWrappedKey(aws_wrapped_key_));

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeyWithMultiThreadsSuccessful) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(-1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(-1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(response));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  // The DEK being decrypted and cached. The cache should be kept, so
  // mock_kms_client_.DecryptSync should only be called once.
  auto decrypted_dek_or = wrapped_key_handler_.GetKey(aws_wrapped_key);

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));

  constexpr auto kNumThreads = 1000;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto decrypted_dek2_or = wrapped_key_handler_.GetKey(aws_wrapped_key);

      EXPECT_THAT(*decrypted_dek2_or, Eq(decrypted_dek));
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest, GetKeyFailedWithInvalidCredentials) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingErrorType::kCustomerKeyPermissionDenied);

  EXPECT_CALL(mock_kms_client_, DecryptSync)
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_INVALID_CREDENTIALS)));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  EXPECT_THAT(wrapped_key_handler_.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_CREDENTIALS)));

  // The key cached as `SC_CPIO_INVALID_CREDENTIALS` in invalid_key_cache.
  EXPECT_THAT(wrapped_key_handler_.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_CREDENTIALS)));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest, GetKeyFailedWithNonRetryableError) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingErrorType::kInvalidKeyId);

  EXPECT_CALL(mock_kms_client_, DecryptSync)
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  EXPECT_THAT(wrapped_key_handler_.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));

  // The key cached as `SC_CPIO_INVALID_ARGUMENT` in invalid_key_cache.
  EXPECT_THAT(wrapped_key_handler_.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GetKeyFailedWithNonRetryableErrorWithMutexLockDisabled) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingErrorType::kInvalidKeyId);

  AwsWrappedKeyHandlerWithCache key_handler_with_mutex_lock_disabled(
      async_executor_, mock_kms_client_, CreateWrappedKeyHandlerOptions(false),
      mock_metric_client_);
  EXPECT_CALL(mock_kms_client_, DecryptSync)
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_INTERNAL_ERROR)));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  EXPECT_THAT(key_handler_with_mutex_lock_disabled.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INTERNAL_ERROR)));

  // The key cached as `SC_CPIO_INTERNAL_ERROR` in invalid_key_cache.
  EXPECT_THAT(key_handler_with_mutex_lock_disabled.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INTERNAL_ERROR)));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       InvalidKeyWithEmptyResponseCachedById) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingErrorType::kInvalidKeyId);

  DecryptResponse empty_response;
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(empty_response));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  EXPECT_THAT(wrapped_key_handler_.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  // We grab the decrypted_dek a second time but kms_client should only be
  // called once (since it's cached after first call).
  EXPECT_THAT(wrapped_key_handler_.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       InvalidKeyWithEmptyResponseCachedByIdWithMutexLockEnabled) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingErrorType::kInvalidKeyId);

  AwsWrappedKeyHandlerWithCache key_handler_with_mutex_lock_enabled(
      async_executor_, mock_kms_client_, CreateWrappedKeyHandlerOptions(true),
      mock_metric_client_);
  DecryptResponse empty_response;
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(empty_response));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  EXPECT_THAT(key_handler_with_mutex_lock_enabled.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  // We grab the decrypted_dek a second time but kms_client should only be
  // called once (since it's cached after first call).
  EXPECT_THAT(key_handler_with_mutex_lock_enabled.GetKey(aws_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDeksByWrappedKeyHandlesTimeout) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(-1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(-1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      -1, KeyFetchingErrorType::kGenericError);

  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce([&response](auto) {
    sleep_for(std::chrono::milliseconds(1000));
    return response;
  });

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  constexpr auto kNumThreads = 10;
  std::atomic<int8_t> success_count = 0;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto decrypted_dek_or = wrapped_key_handler_.GetKey(aws_wrapped_key);
      if (!decrypted_dek_or.result().Successful()) {
        EXPECT_THAT(decrypted_dek_or.result(),
                    ResultIs(FailureExecutionResult(
                        SC_CPIO_KEY_FETCHER_FETCHING_TIMEOUT)));
      }
      if (decrypted_dek_or.has_value() &&
          decrypted_dek_or.value() == decrypted_dek) {
        ++success_count;
      }
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  EXPECT_EQ(success_count.load(), 1);
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GetDecryptionFailuresFromKeyFailureCache) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(-1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(-1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      -1, KeyFetchingErrorType::kInvalidKeyId);

  EXPECT_CALL(mock_kms_client_, DecryptSync)
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  constexpr auto kNumThreads = 100;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      EXPECT_THAT(wrapped_key_handler_.GetKey(aws_wrapped_key).result(),
                  ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessfulWithCacheDisabled) {
  int numOfCalls = 4;
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(numOfCalls);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(numOfCalls);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      numOfCalls, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  AwsWrappedKeyHandlerWithCache key_handler_with_cache_disabled(
      async_executor_, mock_kms_client_,
      CreateWrappedKeyHandlerOptions(/*enable_decryption_lock=*/false,
                                     /*enable_cache=*/false),
      mock_metric_client_);
  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync)
      .Times(numOfCalls)
      .WillRepeatedly(Return(response));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  for (int i = 0; i < numOfCalls; i++) {
    auto decrypted_dek_or =
        key_handler_with_cache_disabled.GetKey(aws_wrapped_key);
    EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
  }
}

TEST_F(AwsWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessfulWithCacheEnabled) {
  int numOfCalls = 4;
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      3, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  AwsWrappedKeyHandlerWithCache key_handler_with_cache_enabled(
      async_executor_, mock_kms_client_,
      CreateWrappedKeyHandlerOptions(/*enable_decryption_lock=*/false,
                                     /*enable_cache=*/true),
      mock_metric_client_);
  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(response));

  auto aws_wrapped_key = BuildCloudWrappedKey(aws_wrapped_key_);
  for (int i = 0; i < numOfCalls; i++) {
    auto decrypted_dek_or =
        key_handler_with_cache_enabled.GetKey(aws_wrapped_key);
    EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
  }
}

}  // namespace google::scp::cpio
