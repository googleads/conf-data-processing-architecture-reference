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

#include "public/cpio/utils/key_fetching/src/ondemand_key_fetcher_with_cache.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "cc/core/test/utils/scp_test_base.h"
#include "cc/public/cpio/utils/key_fetching/proto/encryption_key_prefetch_config.pb.h"
#include "cc/public/cpio/utils/key_fetching/proto/key_coordinator_configuration.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/util/message_differencer.h"
#include "google/protobuf/util/time_util.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/error_codes.h"
#include "public/cpio/mock/private_key_client/mock_private_key_client.h"
#include "public/cpio/utils/dual_writing_metric_client/mock/dual_writing_metric_client_mock.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"
#include "public/cpio/utils/key_fetching/test/key_fetching_metric_matcher.h"

using google::cmrt::sdk::private_key_service::v1::
    ListActiveEncryptionKeysRequest;
using google::cmrt::sdk::private_key_service::v1::
    ListActiveEncryptionKeysResponse;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::cmrt::sdk::v1::EncryptionKeyPrefetchConfig;
using google::cmrt::sdk::v1::KeyCoordinatorConfiguration;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::errors::SC_CPIO_ENTITY_NOT_FOUND;
using google::scp::core::errors::SC_CPIO_KEY_COUNT_MISMATCH;
using google::scp::core::errors::SC_CPIO_KEY_FETCHER_FETCHING_TIMEOUT;
using google::scp::core::errors::SC_CPIO_KEY_NOT_FOUND;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::cpio::DualWritingMetricClientMock;
using google::scp::cpio::ExpectOtelKeyAgeInDaysMetricPush;
using google::scp::cpio::ExpectOtelKeyCacheStatusMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingErrorMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingLatencyMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingRequestMetricPush;
using google::scp::cpio::KeyCacheStatus;
using google::scp::cpio::KeyFetchingType;
using google::scp::cpio::KeyType;
using google::scp::cpio::MockPrivateKeyClient;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using ::std::placeholders::_1;
using std::this_thread::sleep_for;
using testing::_;
using testing::AtLeast;
using ::testing::Eq;
using testing::FieldsAre;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;

namespace google::scp::cpio {
namespace {
constexpr char kEndpoint1[] = "endpoint1";
constexpr char kCloudFunctionUrl1[] = "cloud_function_url1";
constexpr char kWipp1[] = "gcp_wip_provider1";
constexpr char kEndpoint2[] = "endpoint2";
constexpr char kCloudFunctionUrl2[] = "cloud_function_url2";
constexpr char kWipp2[] = "gcp_wip_provider2";
constexpr char kInputKeyId1[] = "output_key_1";
constexpr char kInputKeyId2[] = "output_key_2";
constexpr char kPrivateKey[] = "private_data";
constexpr char kPublicKey[] = "public_data";
constexpr char kKeyNamespace1[] = "keyset1";
constexpr char kKeyNamespace2[] = "keyset2";
constexpr char kAllKeyNamespaces[] = "keyset1_keyset2";
constexpr char kInvalidInputKeyId[] = "invalid_input_key";

KeyCoordinatorConfiguration CreateKeyServiceOptions() {
  KeyCoordinatorConfiguration options;
  options.add_key_namespace(kKeyNamespace1);
  options.add_key_namespace(kKeyNamespace2);
  auto* coordinator_1 = options.add_private_key_endpoints();
  coordinator_1->set_endpoint(kEndpoint1);
  coordinator_1->set_gcp_wip_provider(kWipp1);
  coordinator_1->set_gcp_cloud_function_url(kCloudFunctionUrl1);

  auto* coordinator_2 = options.add_private_key_endpoints();
  coordinator_2->set_endpoint(kEndpoint2);
  coordinator_2->set_gcp_wip_provider(kWipp2);
  coordinator_2->set_gcp_cloud_function_url(kCloudFunctionUrl2);

  return options;
}

PrivateKey CreatePrivateKey1(nanoseconds key_create_ts) {
  PrivateKey output_key_1;
  *output_key_1.mutable_key_id() = kInputKeyId1;
  *output_key_1.mutable_private_key() = kPrivateKey;
  *output_key_1.mutable_key_set_name() = kKeyNamespace1;
  *output_key_1.mutable_public_key() = kPublicKey;
  auto create_time = TimeUtil::NanosecondsToTimestamp(key_create_ts.count());
  *output_key_1.mutable_creation_time() = create_time;
  *output_key_1.mutable_activation_time() = create_time;
  return output_key_1;
}
}  // namespace

MATCHER_P(EqualsProtoIgnoringTimeRange, expected_proto, "") {
  const auto& actual_proto = arg;

  google::protobuf::util::MessageDifferencer differ;
  differ.set_message_field_comparison(
      google::protobuf::util::MessageDifferencer::EQUIVALENT);

  const google::protobuf::FieldDescriptor* time_range_field =
      actual_proto.GetDescriptor()->FindFieldByName("query_time_range");

  if (time_range_field) {
    differ.IgnoreField(time_range_field);
  }

  return differ.Compare(actual_proto, expected_proto);
}

class OndemandKeyFetcherWithCacheTest : public ScpTestBase {
 public:
  OndemandKeyFetcherWithCacheTest()
      : async_executor_(make_shared<AsyncExecutor>(2, 10)) {
    KeyFetcherOptions default_options = {
        .enable_active_keys_api_for_encryption_keys = false};
    key_fetcher_with_cache_ = make_unique<OndemandKeyFetcherWithCache>(
        async_executor_, mock_key_client_, mock_metric_client_,
        CreateKeyServiceOptions(), default_options);
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());
    EXPECT_SUCCESS(key_fetcher_with_cache_->Init());
    EXPECT_SUCCESS(key_fetcher_with_cache_->Run());
  }

 protected:
  void SetUp() override {
    auto* endpoint1 = request_.add_key_endpoints();
    endpoint1->set_endpoint(kEndpoint1);
    endpoint1->set_gcp_cloud_function_url(kCloudFunctionUrl1);
    endpoint1->set_gcp_wip_provider(kWipp1);
    auto* endpoint2 = request_.add_key_endpoints();
    endpoint2->set_endpoint(kEndpoint2);
    endpoint2->set_gcp_cloud_function_url(kCloudFunctionUrl2);
    endpoint2->set_gcp_wip_provider(kWipp2);
  }

  void TearDown() override {
    EXPECT_SUCCESS(key_fetcher_with_cache_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
  }

  void ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      int call_count,
      absl::string_view key_fetching_type = KeyFetchingType::kOnDemand,
      absl::string_view keyset_name = kKeyNamespace1,
      absl::string_view error_code = KeyFetchingErrorType::kGenericError) {
    ExpectOtelKeyFetchingErrorMetricPush(
        mock_metric_client_, call_count, KeyType::kEncryptionKey,
        key_fetching_type, keyset_name, error_code);
  }

  void ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      int call_count,
      absl::string_view key_fetching_type = KeyFetchingType::kOnDemand,
      absl::string_view keyset_name = kKeyNamespace1) {
    ExpectOtelKeyFetchingRequestMetricPush(mock_metric_client_, call_count,
                                           KeyType::kEncryptionKey,
                                           key_fetching_type, keyset_name);
  }

  void ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      int call_count,
      absl::string_view key_fetching_type = KeyFetchingType::kOnDemand,
      absl::string_view keyset_name = kKeyNamespace1) {
    ExpectOtelKeyFetchingLatencyMetricPush(mock_metric_client_, call_count,
                                           KeyType::kEncryptionKey,
                                           key_fetching_type, keyset_name);
  }

  void ExpectOtelEncryptionKeyCacheStatusMetricPush(
      int call_count, absl::string_view keyset_name = kKeyNamespace1,
      absl::string_view key_cache_status = KeyCacheStatus::kValidKeyCacheHit) {
    ExpectOtelKeyCacheStatusMetricPush(mock_metric_client_, call_count,
                                       KeyType::kEncryptionKey, keyset_name,
                                       key_cache_status);
  }

  void ExpectOtelEncryptionKeyAgeInDaysMetricPush(
      int call_count, absl::string_view keyset_name = kKeyNamespace1,
      int16_t key_age_in_days = 0) {
    ExpectOtelKeyAgeInDaysMetricPush(
        mock_metric_client_, call_count, KeyType::kEncryptionKey,
        KeyFetchingType::kOnDemand, keyset_name, key_age_in_days);
  }

  ListPrivateKeysRequest CreateBasePrivateKeysRequest(
      const std::string& keyset_name) {
    ListPrivateKeysRequest request;
    request.set_key_set_name(keyset_name);
    auto* endpoint1 = request.add_key_endpoints();
    endpoint1->set_endpoint(kEndpoint1);
    endpoint1->set_gcp_cloud_function_url(kCloudFunctionUrl1);
    endpoint1->set_gcp_wip_provider(kWipp1);
    auto* endpoint2 = request.add_key_endpoints();
    endpoint2->set_endpoint(kEndpoint2);
    endpoint2->set_gcp_cloud_function_url(kCloudFunctionUrl2);
    endpoint2->set_gcp_wip_provider(kWipp2);
    return request;
  }

  ListActiveEncryptionKeysRequest CreateBaseActiveKeysRequest(
      const std::string& keyset_name) {
    ListActiveEncryptionKeysRequest request;
    request.set_key_set_name(keyset_name);
    auto* endpoint1 = request.add_key_endpoints();
    endpoint1->set_endpoint(kEndpoint1);
    endpoint1->set_gcp_cloud_function_url(kCloudFunctionUrl1);
    endpoint1->set_gcp_wip_provider(kWipp1);
    auto* endpoint2 = request.add_key_endpoints();
    endpoint2->set_endpoint(kEndpoint2);
    endpoint2->set_gcp_cloud_function_url(kCloudFunctionUrl2);
    endpoint2->set_gcp_wip_provider(kWipp2);
    return request;
  }

  shared_ptr<AsyncExecutorInterface> async_executor_;
  MockPrivateKeyClient mock_key_client_;
  unique_ptr<OndemandKeyFetcherWithCache> key_fetcher_with_cache_;
  ListPrivateKeysRequest request_;
  NiceMock<DualWritingMetricClientMock> mock_metric_client_;
};

TEST_F(OndemandKeyFetcherWithCacheTest, GettingKeysSuccessfulById) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(1, kKeyNamespace1, 0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response;
  response.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  request_.add_key_ids(kInputKeyId1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(response));

  auto key = key_fetcher_with_cache_->GetKey(kInputKeyId1);

  EXPECT_THAT(key->creation_timestamp, key_create_ts.count());
  EXPECT_THAT(key->private_key, kPrivateKey);
  EXPECT_THAT(key->public_key, kPublicKey);

  // Gets the key for the second time and the key_client only being called once.
  auto key2 = key_fetcher_with_cache_->GetKey(kInputKeyId1);

  EXPECT_THAT(key2->creation_timestamp, key_create_ts.count());
  EXPECT_THAT(key2->private_key, kPrivateKey);
  EXPECT_THAT(key2->public_key, kPublicKey);
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       GettingKeysSuccessfulByIdWithMultiThreads) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      -1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      -1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(-1, kKeyNamespace1, 0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response;
  response.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  request_.add_key_ids(kInputKeyId1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(response));

  // The key being fetched and cached. The cache should be kept, so
  // mock_mock_key_client_.ListPrivateKeysSync should only be called once.
  auto key = key_fetcher_with_cache_->GetKey(kInputKeyId1);

  EXPECT_THAT(key->creation_timestamp, key_create_ts.count());
  EXPECT_THAT(key->private_key, kPrivateKey);
  EXPECT_THAT(key->public_key, kPublicKey);

  constexpr auto kNumThreads = 100;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto key2 = key_fetcher_with_cache_->GetKey(kInputKeyId1);

      EXPECT_THAT(key2->creation_timestamp, key_create_ts.count());
      EXPECT_THAT(key2->private_key, kPrivateKey);
      EXPECT_THAT(key2->public_key, kPublicKey);
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       GettingKeysSuccessfulByIdWithAllowedKeysetName) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      2, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      2, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      2, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(-1, kKeyNamespace1, 0);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(-1, kKeyNamespace2, 0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response_1;
  response_1.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  ListPrivateKeysRequest request_1;
  request_1.CopyFrom(request_);
  request_1.add_key_ids(kInputKeyId1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_1)))
      .WillOnce(Return(response_1));

  auto key_1 = key_fetcher_with_cache_->GetKey(kInputKeyId1);

  EXPECT_THAT(key_1->creation_timestamp, key_create_ts.count());
  EXPECT_THAT(key_1->private_key, kPrivateKey);
  EXPECT_THAT(key_1->public_key, kPublicKey);

  PrivateKey output_key_2;
  *output_key_2.mutable_key_id() = kInputKeyId2;
  *output_key_2.mutable_key_set_name() = kKeyNamespace2;
  *output_key_2.mutable_private_key() = kPrivateKey;
  *output_key_2.mutable_public_key() = kPublicKey;
  auto create_time = TimeUtil::NanosecondsToTimestamp(key_create_ts.count());
  *output_key_2.mutable_creation_time() = create_time;
  *output_key_2.mutable_activation_time() = create_time;
  ListPrivateKeysResponse response_2;
  response_2.mutable_private_keys()->Add(std::move(output_key_2));
  ListPrivateKeysRequest request_2;
  request_2.CopyFrom(request_);
  request_2.add_key_ids(kInputKeyId2);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_2)))
      .WillOnce(Return(response_2));

  auto key_2 = key_fetcher_with_cache_->GetKey(kInputKeyId2);

  EXPECT_THAT(key_2->creation_timestamp, key_create_ts.count());
  EXPECT_THAT(key_2->private_key, kPrivateKey);
  EXPECT_THAT(key_2->public_key, kPublicKey);
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       GettingKeysByIdFailedWithUnallowedKeysetName) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kInvalidKeyId);

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  PrivateKey output_key_1;
  *output_key_1.mutable_key_set_name() = "wrong_keyset";
  *output_key_1.mutable_key_id() = kInputKeyId1;
  *output_key_1.mutable_private_key() = kPrivateKey;
  *output_key_1.mutable_public_key() = kPublicKey;
  *output_key_1.mutable_creation_time() =
      TimeUtil::NanosecondsToTimestamp(key_create_ts.count());
  ListPrivateKeysResponse response;
  response.mutable_private_keys()->Add(std::move(output_key_1));
  request_.add_key_ids(kInputKeyId1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(response));

  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInputKeyId1),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  // The key from unauthorized key set cached as `SC_CPIO_KEY_NOT_FOUND` in
  // invalid_key_cache.
  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInputKeyId1),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       GettingKeysByIdFailedWithUntryableKeyFetchingError) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kInvalidKeyId);

  request_.add_key_ids(kInputKeyId1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_ENTITY_NOT_FOUND)));

  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInputKeyId1),
              ResultIs(FailureExecutionResult(SC_CPIO_ENTITY_NOT_FOUND)));

  // The key from unauthorized key set cached as `SC_CPIO_ENTITY_NOT_FOUND` in
  // invalid_key_cache.
  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInputKeyId1),
              ResultIs(FailureExecutionResult(SC_CPIO_ENTITY_NOT_FOUND)));
}

TEST_F(OndemandKeyFetcherWithCacheTest, InvalidKeyWithEmptyResponseCachedById) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kInvalidKeyId);

  request_.add_key_ids(kInvalidInputKeyId);
  ListPrivateKeysResponse empty_response;
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(empty_response));

  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInvalidInputKeyId),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  // The key is fetched a second time but key_client will only be called once.
  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInvalidInputKeyId),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       InvalidKeyWithMismatchedKeyCountCachedById) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kGenericError);

  request_.add_key_ids(kInvalidInputKeyId);
  ListPrivateKeysResponse response;
  response.add_private_keys();
  response.add_private_keys();
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(response));

  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInvalidInputKeyId),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_COUNT_MISMATCH)));

  // The key is fetched a second time but key_client will only be called once.
  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInvalidInputKeyId),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_COUNT_MISMATCH)));
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       PrefetchingKeysWorksAndFurtherCallsUseCachedResult) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      2, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  seconds max_age_seconds(5);
  // Make a new KeyFetcher with the proper settings
  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      CreateKeyServiceOptions(),
      KeyFetcherOptions{.prefetch_keys = true,
                        .prefetch_keys_max_age = max_age_seconds,
                        .enable_active_keys_api_for_encryption_keys = false});

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response;
  response.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  request_.set_max_age_seconds(max_age_seconds.count());
  request_.set_key_set_name(kKeyNamespace1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(response));
  request_.set_key_set_name(kKeyNamespace2);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(ListPrivateKeysResponse()));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());

  auto key = prefetching_key_fetcher_with_cache.GetKey(kInputKeyId1);

  EXPECT_THAT(key->creation_timestamp, key_create_ts.count());
  EXPECT_THAT(key->private_key, kPrivateKey);
  EXPECT_THAT(key->public_key, kPublicKey);

  // Gets the key for the second time and the key_client only being called once.
  auto key2 = prefetching_key_fetcher_with_cache.GetKey(kInputKeyId1);

  EXPECT_THAT(key2->creation_timestamp, key_create_ts.count());
  EXPECT_THAT(key2->private_key, kPrivateKey);
  EXPECT_THAT(key2->public_key, kPublicKey);

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Stop());
}

TEST_F(OndemandKeyFetcherWithCacheTest, PrefetchListingKeysFailsNoRetry) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1,
      KeyFetchingErrorType::kGenericError);

  seconds max_age_seconds(5);
  // Make a new KeyFetcher with the proper settings
  auto options = CreateKeyServiceOptions();
  options.mutable_key_namespace()->RemoveLast();
  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_, options,
      KeyFetcherOptions{.prefetch_keys = true,
                        .prefetch_retry = false,
                        .max_prefetch_wait_time_millis = 1,
                        .prefetch_keys_max_age = max_age_seconds,
                        .enable_active_keys_api_for_encryption_keys = false});

  request_.set_max_age_seconds(max_age_seconds.count());
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync)
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());
  prefetching_key_fetcher_with_cache.Stop();
}

TEST_F(OndemandKeyFetcherWithCacheTest, PrefetchListingKeysFailsWithRetry) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1,
      KeyFetchingErrorType::kGenericError);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetchRetry, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetchRetry, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetchRetry, kKeyNamespace1,
      KeyFetchingErrorType::kGenericError);

  seconds max_age_seconds(5);
  // Make a new KeyFetcher with the proper settings

  auto options = CreateKeyServiceOptions();
  options.mutable_key_namespace()->RemoveLast();
  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_, options,
      KeyFetcherOptions{.prefetch_keys = true,
                        .prefetch_retry = true,
                        .max_prefetch_wait_time_millis = 1,
                        .prefetch_keys_max_age = max_age_seconds,
                        .enable_active_keys_api_for_encryption_keys = false});

  request_.set_max_age_seconds(max_age_seconds.count());
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync)
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());
  prefetching_key_fetcher_with_cache.Stop();
}

TEST_F(OndemandKeyFetcherWithCacheTest, OndemandFetchingOnceWithMultiThreads) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(1, kKeyNamespace1, 0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  OndemandKeyFetcherWithCache key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      CreateKeyServiceOptions(),
      KeyFetcherOptions{.enable_on_demand_fetching_lock_for_encryption_key =
                            true});
  EXPECT_SUCCESS(key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(key_fetcher_with_cache.Run());

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response;
  response.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  request_.add_key_ids(kInputKeyId1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(response));

  constexpr auto kNumThreads = 1000;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto key = key_fetcher_with_cache.GetKey(kInputKeyId1);

      EXPECT_THAT(key->creation_timestamp, key_create_ts.count());
      EXPECT_THAT(key->private_key, kPrivateKey);
      EXPECT_THAT(key->public_key, kPublicKey);
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  EXPECT_SUCCESS(key_fetcher_with_cache.Stop());
}

TEST_F(OndemandKeyFetcherWithCacheTest, OndemandFetchingTimeout) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyAgeInDaysMetricPush(1, kKeyNamespace1, 0);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      -1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kGenericError);

  OndemandKeyFetcherWithCache key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      CreateKeyServiceOptions(),
      KeyFetcherOptions{
          .enable_on_demand_fetching_lock_for_encryption_key = true,
          .on_demand_fetching_waiting_timeout = std::chrono::milliseconds(20)});
  EXPECT_SUCCESS(key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(key_fetcher_with_cache.Run());

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response;
  response.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  request_.add_key_ids(kInputKeyId1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce([&response](ListPrivateKeysRequest request) {
        sleep_for(std::chrono::milliseconds(1000));
        return response;
      });

  constexpr auto kNumThreads = 10;
  int8_t success_count = 0;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto key = key_fetcher_with_cache.GetKey(kInputKeyId1);
      if (!key.result().Successful()) {
        EXPECT_THAT(key.result(), ResultIs(FailureExecutionResult(
                                      SC_CPIO_KEY_FETCHER_FETCHING_TIMEOUT)));
      }
      if (key->creation_timestamp == key_create_ts.count() &&
          key->private_key == kPrivateKey && key->public_key == kPublicKey) {
        ++success_count;
      }
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  EXPECT_THAT(success_count, 1);
  EXPECT_SUCCESS(key_fetcher_with_cache.Stop());
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       OndemandFetchingOnceWithKeyFoundInInvalidCache) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      -1, kAllKeyNamespaces, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kInvalidKeyId);

  OndemandKeyFetcherWithCache key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      CreateKeyServiceOptions(),
      KeyFetcherOptions{.enable_on_demand_fetching_lock_for_encryption_key =
                            true});
  EXPECT_SUCCESS(key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(key_fetcher_with_cache.Run());

  request_.add_key_ids(kInputKeyId1);
  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(EqualsProto(request_)))
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  constexpr auto kNumThreads = 1000;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      EXPECT_THAT(key_fetcher_with_cache.GetKey(kInputKeyId1).result(),
                  ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  EXPECT_SUCCESS(key_fetcher_with_cache.Stop());
}

class OndemandKeyFetcherWithCacheActiveTest
    : public OndemandKeyFetcherWithCacheTest {
 public:
  OndemandKeyFetcherWithCacheActiveTest() {
    if (key_fetcher_with_cache_) {
      EXPECT_SUCCESS(key_fetcher_with_cache_->Stop());
    }
    if (async_executor_) {
      EXPECT_SUCCESS(async_executor_->Stop());
    }

    async_executor_ = make_shared<AsyncExecutor>(2, 10);
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());

    seconds max_age_seconds(5);
    KeyFetcherOptions default_options = {
        .prefetch_keys = true,
        .max_prefetch_wait_time_millis = 1,
    };

    EncryptionKeyPrefetchConfig::KeysetPrefetchConfig config1;
    config1.mutable_prefetch_duration()->set_seconds(max_age_seconds.count());
    default_options.encryption_key_prefetch_config_map[kKeyNamespace1] =
        config1;

    EncryptionKeyPrefetchConfig::KeysetPrefetchConfig config2;
    config2.mutable_prefetch_duration()->set_seconds(max_age_seconds.count());
    default_options.encryption_key_prefetch_config_map[kKeyNamespace2] =
        config2;

    key_fetcher_with_cache_ = make_unique<OndemandKeyFetcherWithCache>(
        async_executor_, mock_key_client_, mock_metric_client_,
        CreateKeyServiceOptions(), default_options);
    EXPECT_SUCCESS(key_fetcher_with_cache_->Init());
  }
};

TEST_F(OndemandKeyFetcherWithCacheActiveTest,
       PrefetchingActiveKeysSuccessfully) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheHit);

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListActiveEncryptionKeysResponse response1;
  response1.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(EqualsProtoIgnoringTimeRange(
                  CreateBaseActiveKeysRequest(kKeyNamespace1))))
      .WillOnce(Return(response1));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(EqualsProtoIgnoringTimeRange(
                  CreateBaseActiveKeysRequest(kKeyNamespace2))))
      .WillOnce(Return(ListActiveEncryptionKeysResponse()));

  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());

  auto key = key_fetcher_with_cache_->GetKey(kInputKeyId1);
  ASSERT_TRUE(key.Successful());
  EXPECT_THAT(key->creation_timestamp, key_create_ts.count());
}

TEST_F(OndemandKeyFetcherWithCacheActiveTest, PrefetchingActiveKeysFails) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1,
      KeyFetchingErrorType::kGenericError);

  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2,
      KeyFetchingErrorType::kGenericError);

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(EqualsProtoIgnoringTimeRange(
                  CreateBaseActiveKeysRequest(kKeyNamespace1))))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(EqualsProtoIgnoringTimeRange(
                  CreateBaseActiveKeysRequest(kKeyNamespace2))))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());

  // NOTE: On-demand fetch currently uses the old ListPrivateKeys API even when
  // enable_active_keys_api_for_encryption_keys is true.
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kInvalidKeyId);

  ListPrivateKeysRequest expected_ondemand_request;
  for (const auto& endpoint : request_.key_endpoints()) {
    *expected_ondemand_request.add_key_endpoints() = endpoint;
  }
  expected_ondemand_request.add_key_ids(kInputKeyId1);

  EXPECT_CALL(mock_key_client_,
              ListPrivateKeysSync(EqualsProto(expected_ondemand_request)))
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInputKeyId1),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kInvalidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      0, KeyFetchingType::kOnDemand, kAllKeyNamespaces);

  EXPECT_CALL(mock_key_client_, ListPrivateKeysSync(_)).Times(0);
  EXPECT_CALL(mock_key_client_, ListActiveEncryptionKeysSync(_)).Times(0);

  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInputKeyId1),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

TEST_F(OndemandKeyFetcherWithCacheActiveTest,
       PrefetchingActiveKeysPartialSuccess) {
  // Namespace1 Success
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);

  // Namespace2 Failure
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2,
      KeyFetchingErrorType::kGenericError);

  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListActiveEncryptionKeysResponse response1;
  response1.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(EqualsProtoIgnoringTimeRange(
                  CreateBaseActiveKeysRequest(kKeyNamespace1))))
      .WillOnce(Return(response1));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(EqualsProtoIgnoringTimeRange(
                  CreateBaseActiveKeysRequest(kKeyNamespace2))))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());

  // Namespace1 is a Cache Hit
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheHit);
  auto key = key_fetcher_with_cache_->GetKey(kInputKeyId1);
  ASSERT_TRUE(key.Successful());
  EXPECT_THAT(key->creation_timestamp, key_create_ts.count());

  // Namespace2 is a Cache Miss
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kInvalidKeyId);

  ListPrivateKeysRequest expected_ondemand_request;
  for (const auto& endpoint : request_.key_endpoints()) {
    *expected_ondemand_request.add_key_endpoints() = endpoint;
  }
  expected_ondemand_request.add_key_ids(
      kInputKeyId2);  // Assuming key 2 would be in NS2

  EXPECT_CALL(mock_key_client_,
              ListPrivateKeysSync(EqualsProto(expected_ondemand_request)))
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInputKeyId2),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

// API Succeeds with Empty Key List
TEST_F(OndemandKeyFetcherWithCacheActiveTest,
       PrefetchingActiveKeysEmptyResponse) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);  // No errors

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(EqualsProtoIgnoringTimeRange(
                  CreateBaseActiveKeysRequest(kKeyNamespace1))))
      .WillOnce(Return(ListActiveEncryptionKeysResponse()));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(EqualsProtoIgnoringTimeRange(
                  CreateBaseActiveKeysRequest(kKeyNamespace2))))
      .WillOnce(Return(ListActiveEncryptionKeysResponse()));

  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());

  // Check key - Cache Miss, triggers on-demand
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kOnDemand, kAllKeyNamespaces,
      KeyFetchingErrorType::kInvalidKeyId);

  ListPrivateKeysRequest expected_ondemand_request;
  for (const auto& endpoint : request_.key_endpoints()) {
    *expected_ondemand_request.add_key_endpoints() = endpoint;
  }
  expected_ondemand_request.add_key_ids(kInputKeyId1);

  EXPECT_CALL(mock_key_client_,
              ListPrivateKeysSync(EqualsProto(expected_ondemand_request)))
      .WillOnce(Return(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  EXPECT_THAT(key_fetcher_with_cache_->GetKey(kInputKeyId1),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
}

TEST_F(OndemandKeyFetcherWithCacheTest, PrefetchWithNewConfigKeyIdAndDuration) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      2, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      2, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  KeyFetcherOptions key_fetcher_options{
      .prefetch_keys = true,
      .prefetch_retry = false,
      .max_prefetch_wait_time_millis = 1,
      .enable_active_keys_api_for_encryption_keys = false,
  };

  EncryptionKeyPrefetchConfig::KeysetPrefetchConfig config1;
  config1.add_key_ids(kInputKeyId1);
  config1.mutable_prefetch_duration()->set_seconds(60 * 60 * 24 * 7);  // 1 week
  key_fetcher_options.encryption_key_prefetch_config_map[kKeyNamespace1] =
      config1;

  auto key_service_options = CreateKeyServiceOptions();
  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      key_service_options, key_fetcher_options);

  ListPrivateKeysRequest expected_private_request1 =
      CreateBasePrivateKeysRequest(kKeyNamespace1);
  expected_private_request1.add_key_ids(kInputKeyId1);
  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response1;
  response1.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  EXPECT_CALL(mock_key_client_,
              ListPrivateKeysSync(EqualsProto(expected_private_request1)))
      .WillOnce(Return(response1));

  ListActiveEncryptionKeysRequest expected_active_request1 =
      CreateBaseActiveKeysRequest(kKeyNamespace1);
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringTimeRange(expected_active_request1)))
      .WillOnce(Return(ListActiveEncryptionKeysResponse()));

  ListPrivateKeysRequest expected_request2 =
      CreateBasePrivateKeysRequest(kKeyNamespace2);
  expected_request2.set_max_age_seconds(
      key_fetcher_options.prefetch_keys_max_age.count());
  EXPECT_CALL(mock_key_client_,
              ListPrivateKeysSync(EqualsProto(expected_request2)))
      .WillOnce(Return(ListPrivateKeysResponse()));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());

  auto key = prefetching_key_fetcher_with_cache.GetKey(kInputKeyId1);
  EXPECT_THAT(key->private_key, kPrivateKey);

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Stop());
}

TEST_F(OndemandKeyFetcherWithCacheTest, PrefetchingFallbackOldListActiveKeys) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kKeyNamespace1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  seconds max_age = std::chrono::seconds(60 * 60 * 24 * 7);  // 1 week

  KeyFetcherOptions key_fetcher_options{
      .prefetch_keys = true,
      .prefetch_retry = false,
      .max_prefetch_wait_time_millis = 1,
      .prefetch_keys_max_age = max_age,
      .enable_active_keys_api_for_encryption_keys = true,
  };

  auto key_service_options = CreateKeyServiceOptions();
  key_service_options.mutable_key_namespace()->Clear();
  key_service_options.add_key_namespace(kKeyNamespace1);
  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      key_service_options, key_fetcher_options);

  ListActiveEncryptionKeysRequest expected_request =
      CreateBaseActiveKeysRequest(kKeyNamespace1);
  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListActiveEncryptionKeysResponse response;
  response.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringTimeRange(expected_request)))
      .WillOnce(Return(response));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());

  auto key = prefetching_key_fetcher_with_cache.GetKey(kInputKeyId1);
  EXPECT_THAT(key->private_key, kPrivateKey);

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Stop());
}

TEST_F(OndemandKeyFetcherWithCacheTest, PrefetchingFallbackOldListPrivateKeys) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kKeyNamespace1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  seconds max_age = std::chrono::seconds(60 * 60 * 24 * 7);

  KeyFetcherOptions key_fetcher_options{
      .prefetch_keys = true,
      .prefetch_retry = false,
      .max_prefetch_wait_time_millis = 1,
      .prefetch_keys_max_age = max_age,
      .enable_active_keys_api_for_encryption_keys = false,
  };

  auto key_service_options = CreateKeyServiceOptions();
  key_service_options.mutable_key_namespace()->Clear();
  key_service_options.add_key_namespace(kKeyNamespace1);
  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      key_service_options, key_fetcher_options);

  ListPrivateKeysRequest expected_request =
      CreateBasePrivateKeysRequest(kKeyNamespace1);
  expected_request.set_max_age_seconds(max_age.count());
  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response;
  response.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  EXPECT_CALL(mock_key_client_,
              ListPrivateKeysSync(EqualsProto(expected_request)))
      .WillOnce(Return(response));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());

  auto key = prefetching_key_fetcher_with_cache.GetKey(kInputKeyId1);
  EXPECT_THAT(key->private_key, kPrivateKey);

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Stop());
}

TEST_F(OndemandKeyFetcherWithCacheTest, PrefetchingWithOldAndNewSystem) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace2);
  ExpectOtelEncryptionKeyCacheStatusMetricPush(
      1, kAllKeyNamespaces, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(0);

  KeyFetcherOptions key_fetcher_options{
      .prefetch_keys = true,
      .prefetch_retry = false,
      .max_prefetch_wait_time_millis = 1,
      .enable_active_keys_api_for_encryption_keys = true,
  };

  EncryptionKeyPrefetchConfig::KeysetPrefetchConfig config1;
  config1.add_key_ids(kInputKeyId1);
  key_fetcher_options.encryption_key_prefetch_config_map[kKeyNamespace1] =
      config1;

  auto key_service_options = CreateKeyServiceOptions();
  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      key_service_options, key_fetcher_options);

  ListPrivateKeysRequest expected_request1 =
      CreateBasePrivateKeysRequest(kKeyNamespace1);
  expected_request1.add_key_ids(kInputKeyId1);
  auto key_create_ts =
      duration_cast<nanoseconds>((system_clock::now()).time_since_epoch());
  ListPrivateKeysResponse response1;
  response1.mutable_private_keys()->Add(CreatePrivateKey1(key_create_ts));
  EXPECT_CALL(mock_key_client_,
              ListPrivateKeysSync(EqualsProto(expected_request1)))
      .WillOnce(Return(response1));

  ListActiveEncryptionKeysRequest expected_request2 =
      CreateBaseActiveKeysRequest(kKeyNamespace2);
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringTimeRange(expected_request2)))
      .WillOnce(Return(ListActiveEncryptionKeysResponse()));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());

  auto key = prefetching_key_fetcher_with_cache.GetKey(kInputKeyId1);
  EXPECT_THAT(key->private_key, kPrivateKey);

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Stop());
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       PrefetchListingKeysFailsNoRetryNewConfig) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1,
      KeyFetchingErrorType::kGenericError);

  seconds max_age_seconds(5);
  // Make a new KeyFetcher with the proper settings
  auto key_service_options = CreateKeyServiceOptions();
  key_service_options.mutable_key_namespace()->Clear();
  key_service_options.add_key_namespace(kKeyNamespace1);

  KeyFetcherOptions key_fetcher_options{
      .prefetch_keys = true,
      .prefetch_retry = false,
      .max_prefetch_wait_time_millis = 1,
      .prefetch_keys_max_age = max_age_seconds};

  EncryptionKeyPrefetchConfig::KeysetPrefetchConfig config1;
  config1.mutable_prefetch_duration()->set_seconds(max_age_seconds.count());

  key_fetcher_options.encryption_key_prefetch_config_map[kKeyNamespace1] =
      config1;

  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      key_service_options, key_fetcher_options);

  ListActiveEncryptionKeysRequest expected_request =
      CreateBaseActiveKeysRequest(kKeyNamespace1);
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringTimeRange(expected_request)))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());
  prefetching_key_fetcher_with_cache.Stop();
}

TEST_F(OndemandKeyFetcherWithCacheTest,
       PrefetchListingKeysFailsWithRetryNewConfig) {
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetch, kKeyNamespace1,
      KeyFetchingErrorType::kGenericError);
  ExpectOtelEncryptionKeyFetchingRequestMetricPush(
      1, KeyFetchingType::kPrefetchRetry, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingLatencyMetricPush(
      1, KeyFetchingType::kPrefetchRetry, kKeyNamespace1);
  ExpectOtelEncryptionKeyFetchingErrorMetricPush(
      1, KeyFetchingType::kPrefetchRetry, kKeyNamespace1,
      KeyFetchingErrorType::kGenericError);

  seconds max_age_seconds(5);
  // Make a new KeyFetcher with the proper settings
  auto key_service_options = CreateKeyServiceOptions();
  key_service_options.mutable_key_namespace()->Clear();
  key_service_options.add_key_namespace(kKeyNamespace1);

  KeyFetcherOptions key_fetcher_options{
      .prefetch_keys = true,
      .prefetch_retry = true,
      .max_prefetch_wait_time_millis = 1,
      .prefetch_keys_max_age = max_age_seconds};

  EncryptionKeyPrefetchConfig::KeysetPrefetchConfig config1;
  config1.mutable_prefetch_duration()->set_seconds(max_age_seconds.count());

  key_fetcher_options.encryption_key_prefetch_config_map[kKeyNamespace1] =
      config1;
  OndemandKeyFetcherWithCache prefetching_key_fetcher_with_cache(
      async_executor_, mock_key_client_, mock_metric_client_,
      key_service_options, key_fetcher_options);

  ListActiveEncryptionKeysRequest expected_request =
      CreateBaseActiveKeysRequest(kKeyNamespace1);
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringTimeRange(expected_request)))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Init());
  EXPECT_SUCCESS(prefetching_key_fetcher_with_cache.Run());
  prefetching_key_fetcher_with_cache.Stop();
}
}  // namespace google::scp::cpio
