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

#include "cc/public/cpio/utils/key_fetching/src/auto_refresh_key_fetcher_with_cache.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "cc/core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "google/protobuf/util/message_differencer.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/private_key_client/mock_private_key_client.h"
#include "public/cpio/utils/dual_writing_metric_client/mock/dual_writing_metric_client_mock.h"
#include "public/cpio/utils/key_fetching/src/error_codes.h"
#include "public/cpio/utils/key_fetching/src/key_fetching_metric_utils.h"
#include "public/cpio/utils/key_fetching/test/key_fetching_metric_matcher.h"
#include "public/cpio/utils/proto_utils.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::MetricType;
using google::cmrt::sdk::private_key_service::v1::GetKeysetMetadataResponse;
using google::cmrt::sdk::private_key_service::v1::
    ListActiveEncryptionKeysRequest;
using google::cmrt::sdk::private_key_service::v1::
    ListActiveEncryptionKeysResponse;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::cmrt::sdk::private_key_service::v1::PrivateKeyEndpoint;
using google::protobuf::util::MessageDifferencer;
using google::protobuf::util::TimeUtil;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::
    SC_CPIO_KEY_FETCHER_INVALID_KEY_SELECTION_TIMESTAMP;
using google::scp::core::errors::SC_CPIO_KEY_FETCHER_NO_KEY_FETCHED;
using google::scp::core::errors::SC_CPIO_KEY_NOT_FOUND;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::SubstituteAndParseTextToProto;
using google::scp::cpio::DualWritingMetricClientMock;
using google::scp::cpio::ExpectOtelKeyCacheStatusMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingErrorMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingLatencyMetricPush;
using google::scp::cpio::ExpectOtelKeyFetchingRequestMetricPush;
using google::scp::cpio::KeyCacheStatus;
using google::scp::cpio::KeyFetchingType;
using google::scp::cpio::KeyType;
using google::scp::cpio::MockPrivateKeyClient;
using google::scp::cpio::ProtoUtils;
using std::make_unique;
using std::string;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;
using testing::ElementsAre;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::FieldsAre;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;
using testing::WithParamInterface;

namespace google::scp::cpio {
namespace {
constexpr char kKeyNamespaceId[] = "test";
constexpr char kEndpoint1[] = "endpoint1";
constexpr char kCloudFunctionUrl1[] = "cloud_function_url1";
constexpr char kWipp1[] = "gcp_wip_provider1";
constexpr char kEndpoint2[] = "endpoint2";
constexpr char kCloudFunctionUrl2[] = "cloud_function_url2";
constexpr char kWipp2[] = "gcp_wip_provider2";
}  // namespace

class AutoRefreshKeyFetcherWithCacheTest : public ScpTestBase {
 public:
  AutoRefreshKeyFetcherWithCacheTest() {
    key_fetcher_with_cache_ = make_unique<AutoRefreshKeyFetcherWithCache>(
        mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
        mock_metric_client_,
        KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                          .auto_refresh_time_duration =
                              std::chrono::seconds(1) /* auto refresh time*/});
    EXPECT_SUCCESS(key_fetcher_with_cache_->Init());
  }

  vector<PrivateKeyEndpoint> CreatePrivateKeyEndpoints() {
    vector<PrivateKeyEndpoint> private_key_endpoints;
    auto& coordinator_1 = private_key_endpoints.emplace_back();
    coordinator_1.set_endpoint(kEndpoint1);
    coordinator_1.set_gcp_wip_provider(kWipp1);
    coordinator_1.set_gcp_cloud_function_url(kCloudFunctionUrl1);

    auto& coordinator_2 = private_key_endpoints.emplace_back();
    coordinator_2.set_endpoint(kEndpoint2);
    coordinator_2.set_gcp_wip_provider(kWipp2);
    coordinator_2.set_gcp_cloud_function_url(kCloudFunctionUrl2);
    return private_key_endpoints;
  }

  nanoseconds GetNowTimestampWithNegativeDiff(
      hours subtract_duration_hours = hours(0)) const {
    auto now_ts = system_clock::now();
    return duration_cast<nanoseconds>(
        (now_ts - subtract_duration_hours).time_since_epoch());
  }

  nanoseconds GetNowTimestampWithPositiveDiff(
      hours hours_diff = hours(0)) const {
    auto now_ts = system_clock::now();
    return duration_cast<nanoseconds>((now_ts + hours_diff).time_since_epoch());
  }

  void ExpectOtelSidKeyFetchingErrorMetricPush(
      int call_count,
      absl::string_view key_fetching_type = KeyFetchingType::kOnDemand,
      absl::string_view error_code = KeyFetchingErrorType::kGenericError,
      absl::string_view keyset = kKeyNamespaceId) {
    ExpectOtelKeyFetchingErrorMetricPush(mock_metric_client_, call_count,
                                         KeyType::kSidKey, key_fetching_type,
                                         keyset, error_code);
  }

  void ExpectOtelSidKeyFetchingRequestMetricPush(
      int call_count,
      absl::string_view key_fetching_type = KeyFetchingType::kOnDemand,
      absl::string_view keyset = kKeyNamespaceId) {
    ExpectOtelKeyFetchingRequestMetricPush(mock_metric_client_, call_count,
                                           KeyType::kSidKey, key_fetching_type,
                                           keyset);
  }

  void ExpectOtelKeysetFetchingRequestMetricPush(
      int call_count, absl::string_view key_fetching_type,
      absl::string_view keyset = kKeyNamespaceId) {
    ExpectOtelKeyFetchingRequestMetricPush(mock_metric_client_, call_count,
                                           KeyType::kKeysetMetadata,
                                           key_fetching_type, keyset);
  }

  void ExpectOtelSidKeyFetchingLatencyMetricPush(
      int call_count,
      absl::string_view key_fetching_type = KeyFetchingType::kOnDemand,
      absl::string_view keyset = kKeyNamespaceId) {
    ExpectOtelKeyFetchingLatencyMetricPush(mock_metric_client_, call_count,
                                           KeyType::kSidKey, key_fetching_type,
                                           keyset);
  }

  void ExpectOtelSidKeyCacheStatusMetricPush(
      int call_count,
      absl::string_view key_cache_status = KeyCacheStatus::kValidKeyCacheHit,
      absl::string_view keyset = kKeyNamespaceId) {
    ExpectOtelKeyCacheStatusMetricPush(mock_metric_client_, call_count,
                                       KeyType::kSidKey, keyset,
                                       key_cache_status);
  }

 protected:
  void SetUp() override {
    request_.set_key_set_name(kKeyNamespaceId);
    auto* endpoint1 = request_.add_key_endpoints();
    endpoint1->set_endpoint(kEndpoint1);
    endpoint1->set_gcp_cloud_function_url(kCloudFunctionUrl1);
    endpoint1->set_gcp_wip_provider(kWipp1);
    auto* endpoint2 = request_.add_key_endpoints();
    endpoint2->set_endpoint(kEndpoint2);
    endpoint2->set_gcp_cloud_function_url(kCloudFunctionUrl2);
    endpoint2->set_gcp_wip_provider(kWipp2);
  }

  MockPrivateKeyClient mock_key_client_;
  NiceMock<DualWritingMetricClientMock> mock_metric_client_;
  unique_ptr<AutoRefreshKeyFetcherWithCache> key_fetcher_with_cache_;
  ListActiveEncryptionKeysRequest request_;
};

MATCHER_P2(EqualsProtoIgnoringFields, expected_proto, field_to_ignore, "") {
  const auto& actual_proto = arg;

  MessageDifferencer differ;
  differ.set_message_field_comparison(MessageDifferencer::EQUIVALENT);

  const google::protobuf::FieldDescriptor* field =
      actual_proto.GetDescriptor()->FindFieldByName(field_to_ignore);

  if (field) {
    differ.IgnoreField(field);
  }

  return differ.Compare(actual_proto, expected_proto);
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       RunStillSucceedIfEmptyKeyFetchingResponse) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetch,
                                          KeyFetchingErrorType::kGenericError);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  ListActiveEncryptionKeysResponse response;
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());
  EXPECT_SUCCESS(key_fetcher_with_cache_->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       GetValidKeysFailedWithEmptyResponse) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetch,
                                          KeyFetchingErrorType::kGenericError);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  ListActiveEncryptionKeysResponse response;
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillRepeatedly(Return(response));

  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());

  EXPECT_THAT(key_fetcher_with_cache_->GetValidKeys(),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
  EXPECT_SUCCESS(key_fetcher_with_cache_->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, RunInvokesKeyClientImmediately) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(R"pb(
        private_keys { key_id: "keyId" }
      )pb");
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());
  EXPECT_SUCCESS(key_fetcher_with_cache_->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       AutoRefreshInvokedWhenActivekeyCountUnmatched) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kAutoRefresh);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kAutoRefresh);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  // Keyset metadata with more than 1 active key to trigger the auto-refresh
  // logic in the refresher thread loop.
  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(2);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));

  absl::Notification notification;
  // Response only has one active key. The auto-refresh logic will keep trying
  // to fetch keys until it gets more than 1 active key, which will trigger the
  // notification and stop the test.
  auto response_1 =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(R"pb(
        private_keys { key_id: "keyId" }
      )pb");
  auto response_2 =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(R"pb(
        private_keys { key_id: "keyId" }
        private_keys { key_id: "keyId2" }
      )pb");
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response_1))  // Happens as part of Run() synchronously.
      .WillOnce([response_2, &notification,
                 this](ListActiveEncryptionKeysRequest request) {
        EXPECT_THAT(
            request.key_endpoints(),
            testing::Pointwise(EqualsProto(), request_.key_endpoints()));
        notification.Notify();
        return response_2;
      });

  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());
  // Wait for the invocation and stop.
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(5)));
  EXPECT_SUCCESS(key_fetcher_with_cache_->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       RefreshAsNoValidKeyCachedInCertainTime) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kAutoRefresh);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelKeysetFetchingRequestMetricPush(-1, KeyFetchingType::kAutoRefresh);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kAutoRefresh);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);

  // Fetched key which will expire soon. No valid key in day 3.
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"pb(
            private_keys {
              key_id: "keyId"
              creation_time { $0 }
              activation_time { $1 }
              expiration_time { $2 }

            }
          )pb",
          ProtoUtils::TextProtoString(TimeUtil::NanosecondsToTimestamp(
              GetNowTimestampWithNegativeDiff().count())),
          ProtoUtils::TextProtoString(TimeUtil::NanosecondsToTimestamp(
              GetNowTimestampWithNegativeDiff().count())),
          ProtoUtils::TextProtoString(TimeUtil::NanosecondsToTimestamp(
              GetNowTimestampWithNegativeDiff().count())));
  absl::Notification notification;
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response))  // Happens as part of Run() synchronously.
      .WillOnce([response, &notification,
                 this](ListActiveEncryptionKeysRequest request) {
        EXPECT_THAT(
            request.key_endpoints(),
            testing::Pointwise(EqualsProto(), request_.key_endpoints()));
        notification.Notify();
        return response;
      })
      .WillRepeatedly(Return(response));

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        // no auto-refresh since the time interval is too long.
                        .auto_refresh_time_duration =
                            std::chrono::seconds(1) /* auto refresh time*/});

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());
  // Wait for the invocation and stop.
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       NoRefreshAsValidKeyCachedInGivenTime) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);

  // Key with an undefined (or null) expiration field, signifying indefinite
  // validity.
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(R"pb(
        private_keys { key_id: "keyId" }
      )pb");
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        // no auto-refresh since the time interval is too long.
                        .auto_refresh_time_duration =
                            std::chrono::seconds(1) /* auto refresh time*/});

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());
  // Wait 5 seconds to ensure the automatic refresh thread is running.
  std::this_thread::sleep_for(seconds(5));
  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       NoRefreshingKeysWithListActiveKeysApi) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(R"pb(
        private_keys { key_id: "keyId" }
      )pb");
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        .auto_refresh_time_duration =
                            std::chrono::seconds(30) /* auto refresh time*/});
  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  std::this_thread::sleep_for(std::chrono::seconds(5));
  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       EnableKeySelectionTimestampValidation) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(2, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);

  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));
  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  keyset_metadata_response.set_backfill_days(2);

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillRepeatedly([&](ListActiveEncryptionKeysRequest request) {
        EXPECT_THAT(
            (TimeUtil::NanosecondsToTimestamp(
                 GetNowTimestampWithNegativeDiff(hours(2 * 24 + 12)).count()) -
             request.query_time_range().start_time())
                .seconds(),
            testing::Lt(10));
        EXPECT_THAT(
            (TimeUtil::NanosecondsToTimestamp(
                 GetNowTimestampWithNegativeDiff(hours(2 * 24 + 12)).count()) -
             request.query_time_range().end_time())
                .seconds(),
            testing::Lt(10));
        EXPECT_THAT(
            request.key_endpoints(),
            testing::Pointwise(EqualsProto(), request_.key_endpoints()));
        return response;
      });  // Happens as part of Run() synchronously.
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        // no auto-refresh since the time interval is too long.
                        .auto_refresh_time_duration =
                            std::chrono::seconds(1), /* auto refresh time*/
                        .enable_key_selection_timestamp_validation = true});

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  // Current active key is available.
  auto keys_all_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(keys_all_or);
  EXPECT_THAT(*keys_all_or, ElementsAre(FieldsAre(
                                key1_create_ts.count(), key1_expiry_ts.count(),
                                key1_active_ts.count(), string("data1"),
                                string("public_data1"), "key1")));
  // Key selection timestamp is within the valid backfill range.
  auto keys_all_2_or = key_fetcher_with_cache->GetValidKeys(
      GetNowTimestampWithNegativeDiff(hours(20)).count());
  EXPECT_SUCCESS(keys_all_2_or);
  EXPECT_THAT(
      *keys_all_2_or,
      ElementsAre(FieldsAre(key1_create_ts.count(), key1_expiry_ts.count(),
                            key1_active_ts.count(), string("data1"),
                            string("public_data1"), "key1")));

  // Key selection timestamp is too new or too old.
  EXPECT_THAT(key_fetcher_with_cache->GetValidKeys(
                  GetNowTimestampWithPositiveDiff(hours(24)).count()),
              ResultIs(FailureExecutionResult(
                  SC_CPIO_KEY_FETCHER_INVALID_KEY_SELECTION_TIMESTAMP)));
  EXPECT_THAT(key_fetcher_with_cache->GetValidKeys(
                  GetNowTimestampWithNegativeDiff(hours(96)).count()),
              ResultIs(FailureExecutionResult(
                  SC_CPIO_KEY_FETCHER_INVALID_KEY_SELECTION_TIMESTAMP)));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, GettingKeysetMetadata) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);

  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));
  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillRepeatedly(
          Return(response));  // Happens as part of Run() synchronously.
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        // no auto-refresh since the time interval is too long.
                        .auto_refresh_time_duration =
                            std::chrono::seconds(1) /* auto refresh time*/});

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  // Verify if keys are present.
  auto keys_all_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(keys_all_or);
  EXPECT_THAT(*keys_all_or, ElementsAre(FieldsAre(
                                key1_create_ts.count(), key1_expiry_ts.count(),
                                key1_active_ts.count(), string("data1"),
                                string("public_data1"), "key1")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       GetValidKeyNumberValidationOnlyForConfiguredKeysets) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch,
                                            "test4");
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch,
                                            "test4");
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch,
                                            "test4");
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheHit,
                                        "test4");
  ExpectOtelSidKeyFetchingErrorMetricPush(0, "test4");

  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(5);

  ListActiveEncryptionKeysRequest request;
  request.CopyFrom(request_);
  request.set_key_set_name("test4");
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request, "query_time_range")))
      .WillRepeatedly(
          Return(response));  // Happens as part of Run() synchronously.
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));

  // Keyset "test4" is not configured for key number validation.
  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), "test4",
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        // no auto-refresh since the time interval is too long.
                        .auto_refresh_time_duration =
                            std::chrono::seconds(1) /* auto refresh time*/});

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  // Verify if keys are present.
  auto keys_all_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(keys_all_or);
  EXPECT_THAT(*keys_all_or, ElementsAre(FieldsAre(
                                key1_create_ts.count(), key1_expiry_ts.count(),
                                key1_active_ts.count(), string("data1"),
                                string("public_data1"), "key1")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       GettingKeysSuccessfulImmediatelyAfterRun) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));  // Happens as part of Run() synchronously.
  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        // no auto-refresh since the time interval is too long.
                        .auto_refresh_time_duration =
                            std::chrono::seconds(1) /* auto refresh time*/});

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  // Verify if keys are present.
  auto keys_all_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(keys_all_or);
  EXPECT_THAT(*keys_all_or, ElementsAre(FieldsAre(
                                key1_create_ts.count(), key1_expiry_ts.count(),
                                key1_active_ts.count(), string("data1"),
                                string("public_data1"), "key1")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, OnDemandKeysFetchingTimeout) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kOnDemand);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kOnDemand);
  ExpectOtelSidKeyCacheStatusMetricPush(0, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyCacheStatusMetricPush(-1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetch,
                                          KeyFetchingErrorType::kGenericError);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
     private_keys {
       key_id: "key1"
       private_key: "data1"
       public_key: "public_data1"
       creation_time {
         $0
       }
       expiration_time {
         $1
       }
       activation_time {
         $2
       }
     }
   )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)))  // Prefetch
      .WillOnce([&response](ListActiveEncryptionKeysRequest request) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return response;
      });

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{
          .auto_refresh_time_duration = seconds(10),
          .enable_on_demand_fetching_for_hmac_key = true,
          .on_demand_fetching_waiting_timeout = std::chrono::milliseconds(10)});

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  auto kNumThreads = 10;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  size_t key_size_sum = 0;
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto keys = key_fetcher_with_cache->GetValidKeys();
      if (keys.Successful()) {
        key_size_sum += keys->size();
      }
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  // Only one thread fetched the keys on time.
  EXPECT_THAT(key_size_sum, 1);
  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, GettingKeysSuccessfulWithOndemand) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kOnDemand);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kOnDemand);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)))  // Prefetch failed
      .WillOnce(Return(response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        // no auto-refresh since the time interval is too long.
                        .auto_refresh_time_duration =
                            std::chrono::seconds(30), /* auto refresh time*/
                        .enable_on_demand_fetching_for_hmac_key = true});

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  auto keys_all_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(keys_all_or);
  EXPECT_THAT(*keys_all_or, ElementsAre(FieldsAre(
                                key1_create_ts.count(), key1_expiry_ts.count(),
                                key1_active_ts.count(), string("data1"),
                                string("public_data1"), "key1")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       GettingKeysSuccessfulWithAutoRefresh) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingRequestMetricPush(-1, KeyFetchingType::kAutoRefresh);
  ExpectOtelSidKeyFetchingLatencyMetricPush(-1, KeyFetchingType::kAutoRefresh);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)))  // Prefetch failed
      .WillRepeatedly(Return(response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{
          .max_prefetch_wait_time_millis = 1,
          .auto_refresh_time_duration =
              std::chrono::seconds(1) /* auto refresh time*/
      });

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());
  // wait for the auto refresh to fetch the key.
  std::this_thread::sleep_for(seconds(5));

  auto keys_all_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(keys_all_or);
  EXPECT_THAT(*keys_all_or, ElementsAre(FieldsAre(
                                key1_create_ts.count(), key1_expiry_ts.count(),
                                key1_active_ts.count(), string("data1"),
                                string("public_data1"), "key1")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, RefreshClearsOldKeys) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingRequestMetricPush(-1, KeyFetchingType::kAutoRefresh);
  ExpectOtelSidKeyFetchingLatencyMetricPush(-1, KeyFetchingType::kAutoRefresh);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key0_create_ts = GetNowTimestampWithNegativeDiff(hours(48));
  auto key0_active_ts = GetNowTimestampWithNegativeDiff(hours(47));
  auto key0_expiry_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto response_0 =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data_old"
      public_key: "public_data_old"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key0_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key0_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key0_active_ts.count())));
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto response_1 =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response_0))
      .WillRepeatedly(Return(response_1));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{
          .max_prefetch_wait_time_millis = 1,
          .auto_refresh_time_duration =
              std::chrono::seconds(1) /* auto refresh time*/
      });

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());
  // wait for the auto refresh to fetch the key.
  std::this_thread::sleep_for(seconds(5));

  auto keys_all_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(keys_all_or);
  EXPECT_THAT(*keys_all_or, ElementsAre(FieldsAre(
                                key1_create_ts.count(), key1_expiry_ts.count(),
                                key1_active_ts.count(), string("data1"),
                                string("public_data1"), "key1")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, GetValidKeysSuccessfully) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  // There cannot be more than one active key at a time.
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(48));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(36));
  auto key1_expiry_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key2_create_ts = key1_active_ts;
  auto key2_active_ts = key1_expiry_ts;
  auto key2_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto key3_create_ts = key2_active_ts;
  auto key3_active_ts = key2_expiry_ts;
  auto key3_expiry_ts = GetNowTimestampWithPositiveDiff(hours(48));

  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
    private_keys {
      key_id: "key2"
      private_key: "data2"
      public_key: "public_data2"
      creation_time {
        $3
      }
      expiration_time {
        $4
      }
      activation_time {
        $5
      }
    }
    private_keys {
      key_id: "key3"
      private_key: "data3"
      public_key: "public_data3"
      creation_time {
        $6
      }
      expiration_time {
        $7
      }
      activation_time {
        $8
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_active_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key3_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key3_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key3_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{
          .max_prefetch_wait_time_millis = 1,
          .auto_refresh_time_duration =
              std::chrono::seconds(1) /* auto refresh time*/
      });
  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  auto active_key_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(active_key_or);
  EXPECT_THAT(
      *active_key_or,
      ElementsAre(FieldsAre(key2_create_ts.count(), key2_expiry_ts.count(),
                            key2_active_ts.count(), string("data2"),
                            string("public_data2"), "key2")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       GetValidKeysFailedWithMissingActiveKey) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  // There are no active keys at a certain point in time.
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(48));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(36));
  auto key1_expiry_ts = GetNowTimestampWithNegativeDiff(hours(24));

  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{
          .max_prefetch_wait_time_millis = 1,
          .auto_refresh_time_duration =
              std::chrono::seconds(1) /* auto refresh time*/
      });
  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  EXPECT_THAT(key_fetcher_with_cache->GetValidKeys(),
              ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, GetMultipleActiveKey) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(2);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  // There are multiple active keys at a certain point in time.
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(48));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(36));
  auto key1_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto key2_create_ts = key1_active_ts;
  auto key2_active_ts = GetNowTimestampWithNegativeDiff(hours(30));
  auto key2_expiry_ts = GetNowTimestampWithPositiveDiff(hours(48));

  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
    private_keys {
      key_id: "key2"
      private_key: "data2"
      public_key: "public_data2"
      creation_time {
        $3
      }
      expiration_time {
        $4
      }
      activation_time {
        $5
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{
          .max_prefetch_wait_time_millis = 1,
          .auto_refresh_time_duration =
              std::chrono::seconds(1) /* auto refresh time*/
      });
  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  auto multiple_active_keys_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(multiple_active_keys_or);
  EXPECT_THAT(
      *multiple_active_keys_or,
      ElementsAre(FieldsAre(key1_create_ts.count(), key1_expiry_ts.count(),
                            key1_active_ts.count(), string("data1"),
                            string("public_data1"), "key1"),
                  FieldsAre(key2_create_ts.count(), key2_expiry_ts.count(),
                            key2_active_ts.count(), string("data2"),
                            string("public_data2"), "key2")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       GetMultipleActiveKeyWithListActiveKeysApi) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  // There are multiple active keys at a certain point in time.
  auto key1_create_ts = GetNowTimestampWithNegativeDiff(hours(48));
  auto key1_active_ts = GetNowTimestampWithNegativeDiff(hours(36));
  auto key1_expiry_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key2_create_ts = key1_active_ts;
  auto key2_active_ts = GetNowTimestampWithNegativeDiff(hours(30));
  auto key2_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));

  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
    private_keys {
      key_id: "key1"
      private_key: "data1"
      public_key: "public_data1"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
    }
    private_keys {
      key_id: "key2"
      private_key: "data2"
      public_key: "public_data2"
      creation_time {
        $3
      }
      expiration_time {
        $4
      }
      activation_time {
        $5
      }
    }
  )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key1_active_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_expiry_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key2_active_ts.count())));

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{
          .max_prefetch_wait_time_millis = 1,
          .auto_refresh_time_duration =
              std::chrono::seconds(1) /* auto refresh time*/
      });
  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  auto multiple_active_keys_or = key_fetcher_with_cache->GetValidKeys();
  EXPECT_SUCCESS(multiple_active_keys_or);
  EXPECT_THAT(
      *multiple_active_keys_or,
      ElementsAre(FieldsAre(key2_create_ts.count(), key2_expiry_ts.count(),
                            key2_active_ts.count(), string("data2"),
                            string("public_data2"), "key2")));

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, GetValidKeysWithMultiThreads) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyCacheStatusMetricPush(-1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyFetchingErrorMetricPush(0);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  auto key_create_ts = GetNowTimestampWithNegativeDiff(hours(24));
  auto key_active_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key_expiry_ts = GetNowTimestampWithPositiveDiff(hours(24));
  auto key = SubstituteAndParseTextToProto<PrivateKey>(
      R"-(
      key_id: "key"
      private_key: "private_data"
      public_key: "public_data"
      creation_time {
        $0
      }
      expiration_time {
        $1
      }
      activation_time {
        $2
      }
  )-",
      ProtoUtils::TextProtoString(
          TimeUtil::NanosecondsToTimestamp(key_create_ts.count())),
      ProtoUtils::TextProtoString(
          TimeUtil::NanosecondsToTimestamp(key_expiry_ts.count())),
      ProtoUtils::TextProtoString(
          TimeUtil::NanosecondsToTimestamp(key_active_ts.count())));

  ListActiveEncryptionKeysResponse response;
  *response.add_private_keys() = key;
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(response));

  // The key being fetched and cached.
  EXPECT_SUCCESS(key_fetcher_with_cache_->Run());

  auto kNumThreads = 10;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto keys = key_fetcher_with_cache_->GetValidKeys();
      EXPECT_THAT(*keys, UnorderedElementsAre(FieldsAre(
                             key_create_ts.count(), key_expiry_ts.count(),
                             key_active_ts.count(), string("private_data"),
                             string("public_data"), string("key"))));
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  EXPECT_SUCCESS(key_fetcher_with_cache_->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       GetValidKeysOnDemandFetchingOnceWithMultiThreads) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kOnDemand);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kOnDemand);
  ExpectOtelSidKeyCacheStatusMetricPush(-1, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyCacheStatusMetricPush(-1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetch,
                                          KeyFetchingErrorType::kGenericError);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));
  ListActiveEncryptionKeysResponse empty_response;
  auto key_create_ts = GetNowTimestampWithNegativeDiff(hours(23));
  auto key_expiry_ts = GetNowTimestampWithPositiveDiff(hours(2));
  auto response =
      SubstituteAndParseTextToProto<ListActiveEncryptionKeysResponse>(
          R"-(
      private_keys {
        key_id: "key2"
        private_key: "private_data2"
        public_key: "public_data2"
        creation_time {
          $0
        }
        expiration_time {
          $1
        }
      }
    )-",
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key_create_ts.count())),
          ProtoUtils::TextProtoString(
              TimeUtil::NanosecondsToTimestamp(key_expiry_ts.count())));
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(empty_response))
      .WillOnce(Return(response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        .auto_refresh_time_duration =
                            std::chrono::seconds(20) /* auto refresh time*/,
                        .enable_on_demand_fetching_for_hmac_key = true});
  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  // No key being fetched and cached during initialization.
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  auto kNumThreads = 1000;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      auto keys_or = key_fetcher_with_cache->GetValidKeys();
      EXPECT_THAT(keys_or, IsSuccessfulAndHolds(ElementsAre(FieldsAre(
                               key_create_ts.count(), key_expiry_ts.count(), 0,
                               string("private_data2"), string("public_data2"),
                               "key2"))));
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest,
       GetValidKeysOnDemandKeysFetchingFailed) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingRequestMetricPush(-1, KeyFetchingType::kOnDemand);
  ExpectOtelSidKeyFetchingLatencyMetricPush(-1, KeyFetchingType::kOnDemand);
  ExpectOtelSidKeyCacheStatusMetricPush(0, KeyCacheStatus::kValidKeyCacheHit);
  ExpectOtelSidKeyCacheStatusMetricPush(-1, KeyCacheStatus::kValidKeyCacheMiss);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetch,
                                          KeyFetchingErrorType::kGenericError);
  ExpectOtelSidKeyFetchingErrorMetricPush(-1, KeyFetchingType::kOnDemand,
                                          KeyFetchingErrorType::kGenericError);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));

  ListActiveEncryptionKeysResponse empty_response;
  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(empty_response))
      .WillRepeatedly(Return(FailureExecutionResult(SC_UNKNOWN)));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.max_prefetch_wait_time_millis = 1,
                        .auto_refresh_time_duration =
                            std::chrono::seconds(20) /* auto refresh time*/,
                        .enable_on_demand_fetching_for_hmac_key = true});
  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  // No key being fetched and cached during initialization.
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());

  auto kNumThreads = 10;
  std::vector<std::thread> work_threads;
  work_threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    work_threads.emplace_back([&] {
      EXPECT_THAT(key_fetcher_with_cache->GetValidKeys(),
                  ResultIs(FailureExecutionResult(SC_CPIO_KEY_NOT_FOUND)));
    });
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

TEST_F(AutoRefreshKeyFetcherWithCacheTest, PrefetchFailsWithRetry) {
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetch);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetch,
                                          KeyFetchingErrorType::kGenericError);
  ExpectOtelSidKeyFetchingRequestMetricPush(1, KeyFetchingType::kPrefetchRetry);
  ExpectOtelSidKeyFetchingLatencyMetricPush(1, KeyFetchingType::kPrefetchRetry);
  ExpectOtelSidKeyFetchingErrorMetricPush(1, KeyFetchingType::kPrefetchRetry,
                                          KeyFetchingErrorType::kGenericError);
  ExpectOtelKeysetFetchingRequestMetricPush(1, KeyFetchingType::kPrefetch);

  GetKeysetMetadataResponse keyset_metadata_response;
  keyset_metadata_response.set_active_key_count(1);
  EXPECT_CALL(mock_key_client_, GetKeysetMetadataSync)
      .WillOnce(Return(keyset_metadata_response));

  auto key_fetcher_with_cache = make_unique<AutoRefreshKeyFetcherWithCache>(
      mock_key_client_, CreatePrivateKeyEndpoints(), kKeyNamespaceId,
      mock_metric_client_,
      KeyFetcherOptions{.prefetch_retry = true,
                        .max_prefetch_wait_time_millis = 1,
                        .auto_refresh_time_duration =
                            std::chrono::seconds(20) /* auto refresh time*/,
                        .enable_on_demand_fetching_for_hmac_key = true});

  EXPECT_CALL(mock_key_client_,
              ListActiveEncryptionKeysSync(
                  EqualsProtoIgnoringFields(request_, "query_time_range")))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(key_fetcher_with_cache->Init());
  EXPECT_SUCCESS(key_fetcher_with_cache->Run());
  EXPECT_SUCCESS(key_fetcher_with_cache->Stop());
}

}  // namespace google::scp::cpio
