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

#include "public/cpio/utils/key_fetching/src/gcp_wrapped_key_handler_with_cache.h"

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
using google::cmrt::sdk::v1::CloudWrappedKey;
using google::cmrt::sdk::v1::GcpWrappedKey;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_CPIO_INTERNAL_ERROR;
using google::scp::core::errors::SC_CPIO_INVALID_ARGUMENT;
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
constexpr absl::string_view kGcpKmsResourceName =
    "projects/test-project/locations/test-location/keyRings/test-keyring/"
    "cryptoKeys/test-key";
constexpr absl::string_view kWipProvider = "wip_provider";

CloudWrappedKey BuildCloudWrappedKey(const GcpWrappedKey& gcp_wrapped_key) {
  CloudWrappedKey cloud_wrapped_key;
  *cloud_wrapped_key.mutable_gcp_wrapped_key() = gcp_wrapped_key;
  return cloud_wrapped_key;
}

WrappedKeyHandlerOptions CreateWrappedKeyHandlerOptions(
    bool enable_decryption_lock = true, bool enable_cache = true) {
  WrappedKeyHandlerOptions options;
  options.enable_decryption_lock = enable_decryption_lock;
  options.enable_cache = enable_cache;
  options.key_cache_lifetime = std::chrono::seconds(1800);
  options.key_failure_cache_lifetime = std::chrono::seconds(300);
  options.key_decryption_waiting_timeout = std::chrono::milliseconds(200);
  return options;
}

}  // namespace

class GcpWrappedKeyHandlerWithCacheTest : public ScpTestBase {
 public:
  GcpWrappedKeyHandlerWithCacheTest()
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
    gcp_wrapped_key_.set_encrypted_dek(kEncryptedDek);
    gcp_wrapped_key_.set_kek_uri(kGcpKmsResourceName);
    gcp_wrapped_key_.set_wip_provider(kWipProvider);

    expected_decrypt_request_.set_ciphertext(kEncryptedDek);
    expected_decrypt_request_.set_key_resource_name(kGcpKmsResourceName);
    expected_decrypt_request_.set_gcp_wip_provider(kWipProvider);
  }

  void TearDown() override {
    EXPECT_SUCCESS(wrapped_key_handler_.Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
  }

  void ExpectOtelEncryptionKeyFetchingRequestMetricPush(int call_count) {
    ExpectOtelKeyFetchingRequestMetricPush(
        mock_metric_client_, call_count, KeyType::kGcpWrappedKey,
        KeyFetchingType::kOnDemand, kDummyLabelValue);
  }

  void ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      int call_count,
      absl::string_view error_code = KeyFetchingErrorType::kGenericError) {
    ExpectOtelKeyFetchingErrorMetricPush(
        mock_metric_client_, call_count, KeyType::kGcpWrappedKey,
        KeyFetchingType::kOnDemand, kDummyLabelValue, error_code);
  }

  void ExpectOtelEncryptionKeyFetchingLatencyMetricPush(int call_count) {
    ExpectOtelKeyFetchingLatencyMetricPush(
        mock_metric_client_, call_count, KeyType::kGcpWrappedKey,
        KeyFetchingType::kOnDemand, kDummyLabelValue);
  }

  void ExpectOtelEncryptionKeyCacheStatusMetricPush(
      int call_count,
      absl::string_view key_cache_status = KeyCacheStatus::kValidKeyCacheHit) {
    ExpectOtelKeyCacheStatusMetricPush(mock_metric_client_, call_count,
                                       KeyType::kGcpWrappedKey,
                                       kDummyLabelValue, key_cache_status);
  }

  std::shared_ptr<AsyncExecutorInterface> async_executor_;
  NiceMock<DualWritingMetricClientMock> mock_metric_client_;
  MockKmsClient mock_kms_client_;
  GcpWrappedKeyHandlerWithCache wrapped_key_handler_;
  GcpWrappedKey gcp_wrapped_key_;
  DecryptRequest expected_decrypt_request_;
};

TEST_F(GcpWrappedKeyHandlerWithCacheTest, GetKeyFailsIfGcpWrappedKeyIsMissing) {
  CloudWrappedKey cloud_wrapped_key;  // Empty, no gcp_wrapped_key

  auto result = wrapped_key_handler_.GetKey(cloud_wrapped_key);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest, GetKeyFailsIfFieldsAreEmpty) {
  gcp_wrapped_key_.clear_encrypted_dek();
  EXPECT_THAT(
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(gcp_wrapped_key_)),
      ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));

  gcp_wrapped_key_.set_encrypted_dek(kEncryptedDek);
  gcp_wrapped_key_.clear_kek_uri();
  EXPECT_THAT(
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(gcp_wrapped_key_)),
      ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));

  gcp_wrapped_key_.set_kek_uri(kGcpKmsResourceName);
  gcp_wrapped_key_.clear_wip_provider();
  EXPECT_THAT(
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(gcp_wrapped_key_)),
      ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
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

  wrapped_key_handler_.GetKey(BuildCloudWrappedKey(gcp_wrapped_key_));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
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
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(gcp_wrapped_key_));

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeyWithKekTinkPrefixSuccessful) {
  gcp_wrapped_key_.set_kek_uri("gcp-kms://" + string(kGcpKmsResourceName));

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
      wrapped_key_handler_.GetKey(BuildCloudWrappedKey(gcp_wrapped_key_));

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessfulWithMutexLockEnabled) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  GcpWrappedKeyHandlerWithCache key_handler_with_mutex_lock_enabled(
      async_executor_, mock_kms_client_, CreateWrappedKeyHandlerOptions(true),
      mock_metric_client_);
  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(response));

  auto decrypted_dek_or = key_handler_with_mutex_lock_enabled.GetKey(
      BuildCloudWrappedKey(gcp_wrapped_key_));

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessfulWithMutexLockDisabled) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  GcpWrappedKeyHandlerWithCache key_handler_with_mutex_lock_disabled(
      async_executor_, mock_kms_client_, CreateWrappedKeyHandlerOptions(false),
      mock_metric_client_);
  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(response));

  auto decrypted_dek_or = key_handler_with_mutex_lock_disabled.GetKey(
      BuildCloudWrappedKey(gcp_wrapped_key_));

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
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

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  // The DEK being decrypted and cached. The cache should be kept, so
  // mock_kms_client_.DecryptSync should only be called once.
  auto decrypted_dek_or = wrapped_key_handler_.GetKey(cloud_wrapped_key);

  EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));

  constexpr auto kNumThreads = 1000;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto decrypted_dek2_or = wrapped_key_handler_.GetKey(cloud_wrapped_key);

      EXPECT_THAT(*decrypted_dek2_or, Eq(decrypted_dek));
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest, GetKeyFailedWithNonRetryableError) {
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

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  EXPECT_THAT(wrapped_key_handler_.GetKey(cloud_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));

  // The key cached as `SC_CPIO_INVALID_ARGUMENT` in invalid_key_cache.
  EXPECT_THAT(wrapped_key_handler_.GetKey(cloud_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INVALID_ARGUMENT)));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
       GetKeyFailedWithNonRetryableErrorWithMutexLockDisabled) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingErrorType::kInvalidKeyId);

  GcpWrappedKeyHandlerWithCache key_handler_with_mutex_lock_disabled(
      async_executor_, mock_kms_client_, CreateWrappedKeyHandlerOptions(false),
      mock_metric_client_);
  EXPECT_CALL(mock_kms_client_, DecryptSync)
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_INTERNAL_ERROR)));

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  EXPECT_THAT(key_handler_with_mutex_lock_disabled.GetKey(cloud_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INTERNAL_ERROR)));

  // The key cached as `SC_CPIO_INTERNAL_ERROR` in invalid_key_cache.
  EXPECT_THAT(key_handler_with_mutex_lock_disabled.GetKey(cloud_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_INTERNAL_ERROR)));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
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

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  EXPECT_THAT(wrapped_key_handler_.GetKey(cloud_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  // We grab the decrypted_dek a second time but kms_client should only be
  // called once (since it's cached after first call).
  EXPECT_THAT(wrapped_key_handler_.GetKey(cloud_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
       InvalidKeyWithEmptyResponseCachedByIdWithMutexLockEnabled) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingErrorType::kInvalidKeyId);

  GcpWrappedKeyHandlerWithCache key_handler_with_mutex_lock_enabled(
      async_executor_, mock_kms_client_, CreateWrappedKeyHandlerOptions(true),
      mock_metric_client_);
  DecryptResponse empty_response;
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(empty_response));

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  EXPECT_THAT(key_handler_with_mutex_lock_enabled.GetKey(cloud_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  // We grab the decrypted_dek a second time but kms_client should only be
  // called once (since it's cached after first call).
  EXPECT_THAT(key_handler_with_mutex_lock_enabled.GetKey(cloud_wrapped_key),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
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

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  constexpr auto kNumThreads = 10;
  std::atomic<int8_t> success_count = 0;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto decrypted_dek_or = wrapped_key_handler_.GetKey(cloud_wrapped_key);
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

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
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

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  constexpr auto kNumThreads = 100;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      EXPECT_THAT(wrapped_key_handler_.GetKey(cloud_wrapped_key).result(),
                  ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessfulWithCacheDisabled) {
  int numOfCalls = 4;
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(numOfCalls);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(numOfCalls);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      numOfCalls, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  GcpWrappedKeyHandlerWithCache key_handler_with_cache_disabled(
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

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  for (int i = 0; i < numOfCalls; i++) {
    auto decrypted_dek_or =
        key_handler_with_cache_disabled.GetKey(cloud_wrapped_key);
    EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
  }
}

TEST_F(GcpWrappedKeyHandlerWithCacheTest,
       GettingDecryptedDekByWrappedKeySuccessfulWithCacheEnabled) {
  int numOfCalls = 4;
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      3, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  GcpWrappedKeyHandlerWithCache key_handler_with_cache_enabled(
      async_executor_, mock_kms_client_,
      CreateWrappedKeyHandlerOptions(/*enable_decryption_lock=*/false,
                                     /*enable_cache=*/true),
      mock_metric_client_);
  std::string decrypted_dek = "decrypted_dek";
  DecryptResponse response;
  response.set_plaintext(decrypted_dek);
  EXPECT_CALL(mock_kms_client_, DecryptSync).WillOnce(Return(response));

  auto cloud_wrapped_key = BuildCloudWrappedKey(gcp_wrapped_key_);
  for (int i = 0; i < numOfCalls; i++) {
    auto decrypted_dek_or =
        key_handler_with_cache_enabled.GetKey(cloud_wrapped_key);
    EXPECT_THAT(*decrypted_dek_or, Eq(decrypted_dek));
  }
}

}  // namespace google::scp::cpio
