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

#include "cpio/client_providers/auth_token_provider/src/gcp/gcp_auth_token_provider.h"

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/async_executor/src/async_executor.h"
#include "core/curl_client/mock/mock_curl_client.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/scp_test_base.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/auth_token_provider/src/gcp/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::errors::
    SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN;
using google::scp::core::errors::
    SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::MockCurlClient;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::GetSessionTokenRequest;
using google::scp::cpio::client_providers::GetSessionTokenResponse;
using std::atomic;
using std::atomic_bool;
using std::bind;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;
using std::chrono::seconds;
using testing::Between;
using testing::Contains;
using testing::EndsWith;
using testing::Eq;
using testing::IsNull;
using testing::Pair;
using testing::Pointee;
using testing::UnorderedElementsAre;

namespace {
constexpr char kTokenServerPath[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/"
    "service-accounts/default/token";
constexpr char kTeeTokenServerPath[] = "http://localhost/v1/token";
constexpr char kMetadataFlavorHeader[] = "Metadata-Flavor";
constexpr char kMetadataFlavorHeaderValue[] = "Google";
constexpr char kAccessTokenMock[] = "b0Aaekm1IeizWZVKoBQQULOiiT_PDcQk";
const int64_t kTokenLifetimeInSeconds = 3600;
constexpr char kIdentityServerPath[] =
    "http://metadata/computeMetadata/v1/instance/service-accounts/default/"
    "identity";
constexpr char kAudience[] = "www.google.com";
constexpr char kTokenType[] = "LIMITED_AWS";
const seconds kExpireTime =
    seconds(TimeUtil::GetCurrentTime().seconds() + 1800);
constexpr int kRetryTime = 5;
const vector<string> kKeyIds = {"test1", "test2"};
}  // namespace

namespace google::scp::cpio::client_providers::test {

class GcpAuthTokenProviderTest : public ScpTestBase,
                                 public testing::WithParamInterface<string> {
 protected:
  GcpAuthTokenProviderTest() : http_client_(make_shared<MockCurlClient>()) {
    io_async_executor_ = make_shared<AsyncExecutor>(2, 1000);
    EXPECT_SUCCESS(io_async_executor_->Init());
    EXPECT_SUCCESS(io_async_executor_->Run());
    auto options = make_shared<AuthTokenProviderOptions>();
    options->enable_token_cache_for_audience_and_signature_keys = true;
    authorizer_provider_ = make_unique<GcpAuthTokenProvider>(
        std::move(options), http_client_, io_async_executor_);
    EXPECT_SUCCESS(authorizer_provider_->Init());
    EXPECT_SUCCESS(authorizer_provider_->Run());
    fetch_token_for_target_audience_context_.request =
        make_shared<GetSessionTokenForTargetAudienceRequest>();
    fetch_token_for_target_audience_context_.request
        ->token_target_audience_uri = make_shared<string>(kAudience);
    finished_ = false;
    callback_ = CreateCallbackForGetSessionToken(finished_);
    start_time_ = TimeUtil::GetCurrentTime();
  }

  ~GcpAuthTokenProviderTest() {
    EXPECT_SUCCESS(io_async_executor_->Stop());
    EXPECT_SUCCESS(authorizer_provider_->Stop());
  }

  string GetResponseBody() { return GetParam(); }

  void ExpectHttpGetCalledForSessionToken(size_t min_times, size_t max_times) {
    EXPECT_CALL(*http_client_, PerformRequest)
        .Times(Between(min_times, max_times))
        .WillRepeatedly([this](auto& http_context) {
          http_context.result = SuccessExecutionResult();
          EXPECT_EQ(http_context.request->method, HttpMethod::GET);
          EXPECT_THAT(http_context.request->path,
                      Pointee(Eq(kTokenServerPath)));
          EXPECT_THAT(http_context.request->headers,
                      Pointee(UnorderedElementsAre(Pair(
                          kMetadataFlavorHeader, kMetadataFlavorHeaderValue))));

          http_context.response = make_shared<HttpResponse>();
          http_context.response->body = BytesBuffer(http_response_);
          http_context.Finish();
          return SuccessExecutionResult();
        });
  }

  std::function<
      void(AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&)>
  CreateCallbackForGetSessionToken(atomic_bool& finished) {
    return [&finished, this](auto& context) {
      auto end_time = TimeUtil::GetCurrentTime();
      EXPECT_SUCCESS(context.result);
      if (!context.response) {
        ADD_FAILURE();
      } else {
        EXPECT_THAT(context.response->session_token,
                    Pointee(Eq(kAccessTokenMock)));
        EXPECT_GE(context.response->expire_time.count(),
                  start_time_.seconds() + kTokenLifetimeInSeconds);
        EXPECT_LE(context.response->expire_time.count(),
                  end_time.seconds() + kTokenLifetimeInSeconds);
      }
      finished = true;
    };
  }

  void ExpectHttpGetCalledForSessionTokenOfTargetAudience(size_t min_times,
                                                          size_t max_times) {
    EXPECT_CALL(*http_client_, PerformRequest)
        .Times(Between(min_times, max_times))
        .WillRepeatedly([this](auto& http_context) {
          http_context.result = SuccessExecutionResult();
          EXPECT_EQ(http_context.request->method, HttpMethod::GET);
          EXPECT_THAT(http_context.request->path,
                      Pointee(Eq(kIdentityServerPath)));
          EXPECT_THAT(
              http_context.request->query,
              Pointee(absl::StrCat("audience=", kAudience, "&format=full")));
          EXPECT_THAT(http_context.request->headers,
                      Pointee(UnorderedElementsAre(Pair(
                          kMetadataFlavorHeader, kMetadataFlavorHeaderValue))));

          http_context.response = make_shared<HttpResponse>();
          http_context.response->body =
              BytesBuffer(encoded_http_response_for_target_audience_);
          http_context.Finish();
          return SuccessExecutionResult();
        });
  }

  static string CreateHttpResponseForTargetAudience(int expire_time) {
    return absl::StrFormat(
        "someheader.%s.signature",
        *Base64Encode(absl::StrFormat(
            "{\"exp\":%lld,\"iss\":\"issuer\",\"aud\":"
            "\"audience\",\"sub\":\"subject\",\"iat\":1672757101}",
            expire_time)));
  }

  std::function<void(AsyncContext<GetSessionTokenForTargetAudienceRequest,
                                  GetSessionTokenResponse>&)>
  CreateCallbackForGetSessionTokenOfTargetAudience(atomic_bool& finished,
                                                   int64_t expire_time) {
    return [&finished, expire_time](auto& context) {
      EXPECT_SUCCESS(context.result);
      EXPECT_EQ(*context.response->session_token,
                CreateHttpResponseForTargetAudience(expire_time));
      EXPECT_EQ(context.response->expire_time.count(), expire_time);

      finished = true;
    };
  }

  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      fetch_token_context_;
  AsyncContext<HttpRequest, HttpRequest> sign_http_request_context_;

  AsyncContext<GetSessionTokenForTargetAudienceRequest, GetSessionTokenResponse>
      fetch_token_for_target_audience_context_;
  AsyncContext<GetTeeSessionTokenRequest, GetSessionTokenResponse>
      fetch_tee_token_context_;

  shared_ptr<MockCurlClient> http_client_;
  shared_ptr<AsyncExecutorInterface> io_async_executor_;
  unique_ptr<GcpAuthTokenProvider> authorizer_provider_;
  std::function<void(
      AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&)>
      callback_;
  atomic_bool finished_;
  protobuf::Timestamp start_time_;
  string http_response_ = absl::StrFormat(
      "{\"access_token\":\"b0Aaekm1IeizWZVKoBQQULOiiT_PDcQk\",\"expires_"
      "in\":%lld,\"token_type\":\"Bearer\"}",
      kTokenLifetimeInSeconds);
  string encoded_http_response_for_target_audience_ =
      CreateHttpResponseForTargetAudience(kExpireTime.count());
};

TEST_F(GcpAuthTokenProviderTest,
       GetSessionTokenSuccessWithValidTokenAndExpireTime) {
  ExpectHttpGetCalledForSessionToken(1, 1);

  fetch_token_context_.callback = callback_;

  authorizer_provider_->GetSessionToken(fetch_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest, GetCachedTokenSuccessfully) {
  ExpectHttpGetCalledForSessionToken(1, 1);

  fetch_token_context_.callback = callback_;
  authorizer_provider_->GetSessionToken(fetch_token_context_);
  WaitUntil([this]() { return finished_.load(); });

  // Http PerformRequest is only called once, so the fetched token is the
  // cached one.
  finished_ = false;
  fetch_token_context_.callback = callback_;
  authorizer_provider_->GetSessionToken(fetch_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest, MultipleThreadGetSessionTokenSuccessfully) {
  ExpectHttpGetCalledForSessionToken(1, 5);

  std::vector<std::thread> threads;
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back(std::thread([this]() {
      AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
          fetch_token_context;
      atomic_bool finished(false);
      fetch_token_context.callback = CreateCallbackForGetSessionToken(finished);

      authorizer_provider_->GetSessionToken(fetch_token_context);
      WaitUntil([&finished]() { return finished.load(); });
    }));
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST_F(GcpAuthTokenProviderTest, GetSessionTokenFailsIfHttpRequestFails) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = FailureExecutionResult(SC_UNKNOWN);
    http_context.Finish();
    return SuccessExecutionResult();
  });

  atomic_bool finished(false);
  fetch_token_context_.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
    finished = true;
  };
  authorizer_provider_->GetSessionToken(fetch_token_context_);

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_F(GcpAuthTokenProviderTest, NullHttpClientProvider) {
  auto auth_token_provider = make_shared<GcpAuthTokenProvider>(
      make_shared<AuthTokenProviderOptions>(), nullptr, io_async_executor_);

  EXPECT_THAT(auth_token_provider->Init(),
              ResultIs(FailureExecutionResult(
                  SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED)));
}

TEST_F(GcpAuthTokenProviderTest, FetchTokenForTargetAudienceSuccessfully) {
  ExpectHttpGetCalledForSessionTokenOfTargetAudience(1, 1);

  fetch_token_for_target_audience_context_.callback =
      CreateCallbackForGetSessionTokenOfTargetAudience(finished_,
                                                       kExpireTime.count());
  authorizer_provider_->GetSessionTokenForTargetAudience(
      fetch_token_for_target_audience_context_);

  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest,
       GetCachedFetchTokenForTargetAudienceSuccessfully) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = SuccessExecutionResult();
    http_context.response = make_shared<HttpResponse>();
    http_context.response->body =
        BytesBuffer(CreateHttpResponseForTargetAudience(1000));
    http_context.Finish();
    return SuccessExecutionResult();
  });

  fetch_token_for_target_audience_context_.callback =
      CreateCallbackForGetSessionTokenOfTargetAudience(finished_, 1000);
  authorizer_provider_->GetSessionTokenForTargetAudience(
      fetch_token_for_target_audience_context_);
  WaitUntil([this]() { return finished_.load(); });

  // Token expires, call PerformRequest again.
  ExpectHttpGetCalledForSessionTokenOfTargetAudience(1, 1);
  finished_ = false;
  fetch_token_for_target_audience_context_.callback =
      CreateCallbackForGetSessionTokenOfTargetAudience(finished_,
                                                       kExpireTime.count());
  authorizer_provider_->GetSessionTokenForTargetAudience(
      fetch_token_for_target_audience_context_);
  WaitUntil([this]() { return finished_.load(); });

  // Token is not expired, don't call PerformRequest.
  finished_ = false;
  fetch_token_for_target_audience_context_.callback =
      CreateCallbackForGetSessionTokenOfTargetAudience(finished_,
                                                       kExpireTime.count());
  authorizer_provider_->GetSessionTokenForTargetAudience(
      fetch_token_for_target_audience_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest,
       MultipleThreadGetSessionTokenForTargetAudienceSuccessfully) {
  ExpectHttpGetCalledForSessionTokenOfTargetAudience(1, 5);

  std::vector<std::thread> threads;
  for (int i = 0; i < 1; ++i) {
    threads.emplace_back(std::thread([this]() {
      AsyncContext<GetSessionTokenForTargetAudienceRequest,
                   GetSessionTokenResponse>
          fetch_token_context;
      fetch_token_context.request =
          make_shared<GetSessionTokenForTargetAudienceRequest>();
      fetch_token_context.request->token_target_audience_uri =
          make_shared<string>(kAudience);
      atomic_bool finished(false);
      fetch_token_context.callback =
          CreateCallbackForGetSessionTokenOfTargetAudience(finished,
                                                           kExpireTime.count());

      authorizer_provider_->GetSessionTokenForTargetAudience(
          fetch_token_context);
      WaitUntil([&finished]() { return finished.load(); });
    }));
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST_F(GcpAuthTokenProviderTest,
       FetchTokenForTargetAudienceFailsIfHttpRequestFails) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = FailureExecutionResult(SC_UNKNOWN);
    http_context.Finish();
    return SuccessExecutionResult();
  });

  atomic_bool finished(false);
  fetch_token_for_target_audience_context_.callback = [&finished](
                                                          auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
    finished = true;
  };
  authorizer_provider_->GetSessionTokenForTargetAudience(
      fetch_token_for_target_audience_context_);

  WaitUntil([&finished]() { return finished.load(); });
}

INSTANTIATE_TEST_SUITE_P(
    BadTokens, GcpAuthTokenProviderTest,
    testing::Values(absl::StrFormat(
                        R"""({
                              "access_token": "INVALID-JSON",
                              "expires_in": %lld,
                              "token_type"
                            })""",
                        kTokenLifetimeInSeconds) /*missing token_type*/
                    ,
                    R"""({
                                           "access_token": "INVALID-JSON",
                                           "token_type": "Bearer"
                                         })""" /*missing field*/,
                    absl::StrFormat(
                        R"""({
                                           "expires_in": %lld,
                                           "token_type": "Bearer"
                                         })""",
                        kTokenLifetimeInSeconds) /*missing field*/,
                    absl::StrFormat(
                        R"""({
                              "access_token": "INVALID-JSON",
                              "expires_in": %lld
                            })""",
                        kTokenLifetimeInSeconds) /*missing field*/));

TEST_P(GcpAuthTokenProviderTest, GetSessionTokenFailsIfBadJson) {
  EXPECT_CALL(*http_client_, PerformRequest)
      .Times(kRetryTime)
      .WillRepeatedly([this](auto& http_context) {
        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(GetResponseBody());
        http_context.Finish();
        return SuccessExecutionResult();
      });

  std::atomic_bool finished(false);
  fetch_token_context_.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    finished = true;
  };
  authorizer_provider_->GetSessionToken(fetch_token_context_);

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_P(GcpAuthTokenProviderTest, FetchTokenForTargetAudienceFailsIfBadJson) {
  EXPECT_CALL(*http_client_, PerformRequest)
      .Times(kRetryTime)
      .WillRepeatedly([this](auto& http_context) {
        http_context.result = SuccessExecutionResult();

        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(GetResponseBody());
        http_context.Finish();
        return SuccessExecutionResult();
      });

  std::atomic_bool finished(false);
  fetch_token_for_target_audience_context_.callback =
      [&finished](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
        finished = true;
      };
  authorizer_provider_->GetSessionTokenForTargetAudience(
      fetch_token_for_target_audience_context_);

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_F(GcpAuthTokenProviderTest, GetTeeSessionTokenSuccessfully) {
  string tee_token = CreateHttpResponseForTargetAudience(kExpireTime.count());
  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([&tee_token](auto& http_context) {
        EXPECT_EQ(http_context.request->method, HttpMethod::POST);
        EXPECT_THAT(http_context.request->path,
                    Pointee(Eq(kTeeTokenServerPath)));
        EXPECT_THAT(http_context.request->headers,
                    Pointee(UnorderedElementsAre(
                        Pair("Content-Type", "application/json"))));
        EXPECT_EQ(http_context.request->body.ToString(),
                  "{\"audience\": \"www.google.com\", \"token_type\": "
                  "\"LIMITED_AWS\"}");

        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(tee_token);
        http_context.Finish();
        return SuccessExecutionResult();
      });
  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);

  fetch_tee_token_context_.callback = [this, &tee_token](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_EQ(*context.response->session_token, tee_token);
    EXPECT_EQ(context.response->expire_time.count(), kExpireTime.count());
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest, GetTeeSessionTokenWithKeyIdsSuccessfully) {
  string tee_token = CreateHttpResponseForTargetAudience(kExpireTime.count());
  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([&tee_token](auto& http_context) {
        EXPECT_EQ(http_context.request->method, HttpMethod::POST);
        EXPECT_THAT(http_context.request->path,
                    Pointee(Eq(kTeeTokenServerPath)));
        EXPECT_THAT(http_context.request->headers,
                    Pointee(UnorderedElementsAre(
                        Pair("Content-Type", "application/json"))));
        EXPECT_EQ(
            http_context.request->body.ToString(),
            "{\"audience\": \"www.google.com\", \"token_type\": "
            "\"LIMITED_AWS\", \"aws_principal_tag_options\": "
            "{\"allowed_principal_tags\": {\"container_image_signatures\": "
            "{\"key_ids\": [\"test1\",\"test2\"]}}}}");

        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(tee_token);
        http_context.Finish();
        return SuccessExecutionResult();
      });
  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);
  fetch_tee_token_context_.request->key_ids =
      make_shared<vector<string>>(kKeyIds);

  fetch_tee_token_context_.callback = [this, &tee_token](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_EQ(*context.response->session_token, tee_token);
    EXPECT_EQ(context.response->expire_time.count(), kExpireTime.count());
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest,
       GetCachedTeeSessionTokenWithKeyIdsSuccessfully) {
  string tee_token = CreateHttpResponseForTargetAudience(kExpireTime.count());
  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([&tee_token](auto& http_context) {
        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(tee_token);
        http_context.Finish();
        return SuccessExecutionResult();
      });

  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);
  fetch_tee_token_context_.request->key_ids =
      make_shared<vector<string>>(kKeyIds);

  fetch_tee_token_context_.callback = [this, &tee_token](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_EQ(*context.response->session_token, tee_token);
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });

  // Call again: token is cached and not expired, so PerformRequest should NOT
  // be called.
  finished_ = false;
  fetch_tee_token_context_.callback = [this, &tee_token](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_EQ(*context.response->session_token, tee_token);
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest, GetTeeSessionTokenWhenCacheDisabledNotCached) {
  auto options = make_shared<AuthTokenProviderOptions>();
  options->enable_token_cache_for_audience_and_signature_keys = false;
  auto disabled_cache_provider = make_unique<GcpAuthTokenProvider>(
      std::move(options), http_client_, io_async_executor_);
  EXPECT_SUCCESS(disabled_cache_provider->Init());
  EXPECT_SUCCESS(disabled_cache_provider->Run());

  string tee_token = CreateHttpResponseForTargetAudience(kExpireTime.count());
  // Expect 2 HTTP calls because cache is disabled.
  EXPECT_CALL(*http_client_, PerformRequest)
      .Times(2)
      .WillRepeatedly([&tee_token](auto& http_context) {
        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(tee_token);
        http_context.Finish();
        return SuccessExecutionResult();
      });

  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);
  fetch_tee_token_context_.request->key_ids =
      make_shared<vector<string>>(kKeyIds);

  fetch_tee_token_context_.callback = [this, &tee_token](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_EQ(*context.response->session_token, tee_token);
    finished_ = true;
  };

  disabled_cache_provider->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });

  // Call again: cache is disabled, so PerformRequest should be called again.
  finished_ = false;
  fetch_tee_token_context_.callback = [this, &tee_token](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_EQ(*context.response->session_token, tee_token);
    finished_ = true;
  };

  disabled_cache_provider->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });

  EXPECT_SUCCESS(disabled_cache_provider->Stop());
}

TEST_F(GcpAuthTokenProviderTest,
       GetTeeSessionTokenDifferentKeyIdsNotCachedTogether) {
  string tee_token1 = CreateHttpResponseForTargetAudience(kExpireTime.count());
  string tee_token2 =
      CreateHttpResponseForTargetAudience(kExpireTime.count() + 100);

  // Expect 2 HTTP calls because the two requests have different key_ids.
  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([&tee_token1](auto& http_context) {
        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(tee_token1);
        http_context.Finish();
        return SuccessExecutionResult();
      })
      .WillOnce([&tee_token2](auto& http_context) {
        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(tee_token2);
        http_context.Finish();
        return SuccessExecutionResult();
      });

  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);
  fetch_tee_token_context_.request->key_ids =
      make_shared<vector<string>>(vector<string>{"key_a"});

  fetch_tee_token_context_.callback = [this, &tee_token1](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_EQ(*context.response->session_token, tee_token1);
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });

  // Call with different key_ids: should fetch from server, not use previous
  // cache
  finished_ = false;
  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);
  fetch_tee_token_context_.request->key_ids =
      make_shared<vector<string>>(vector<string>{"key_b"});

  fetch_tee_token_context_.callback = [this, &tee_token2](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_EQ(*context.response->session_token, tee_token2);
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest,
       GetTeeSessionTokenWhenJwtMissingExpirationKey) {
  string tee_token = absl::StrFormat(
      "header.%s.sig",
      *Base64Encode(
          "{\"iss\":\"issuer\",\"aud\":\"audience\",\"sub\":\"subject\"}"));
  EXPECT_CALL(*http_client_, PerformRequest)
      .Times(kRetryTime)
      .WillRepeatedly([&tee_token](auto& http_context) {
        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(tee_token);
        http_context.Finish();
        return SuccessExecutionResult();
      });

  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);

  fetch_tee_token_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest, GetTeeSessionTokenWhenJwtPayloadNotJson) {
  string tee_token =
      absl::StrFormat("header.%s.sig", *Base64Encode("not-valid-json"));
  EXPECT_CALL(*http_client_, PerformRequest)
      .Times(kRetryTime)
      .WillRepeatedly([&tee_token](auto& http_context) {
        http_context.result = SuccessExecutionResult();
        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(tee_token);
        http_context.Finish();
        return SuccessExecutionResult();
      });

  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);

  fetch_tee_token_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest, GetTeeSessionTokenFailed) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    EXPECT_EQ(http_context.request->method, HttpMethod::POST);
    EXPECT_THAT(http_context.request->path, Pointee(Eq(kTeeTokenServerPath)));
    EXPECT_THAT(http_context.request->headers,
                Pointee(UnorderedElementsAre(
                    Pair("Content-Type", "application/json"))));
    EXPECT_EQ(http_context.request->body.ToString(),
              "{\"audience\": \"www.google.com\", \"token_type\": "
              "\"LIMITED_AWS\"}");

    http_context.result = FailureExecutionResult(SC_UNKNOWN);
    http_context.Finish();
    return SuccessExecutionResult();
  });
  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);

  fetch_tee_token_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST_F(GcpAuthTokenProviderTest, GetTeeSessionTokenFailedDueToEmptyToken) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    EXPECT_EQ(http_context.request->method, HttpMethod::POST);
    EXPECT_THAT(http_context.request->path, Pointee(Eq(kTeeTokenServerPath)));
    EXPECT_THAT(http_context.request->headers,
                Pointee(UnorderedElementsAre(
                    Pair("Content-Type", "application/json"))));
    EXPECT_EQ(http_context.request->body.ToString(),
              "{\"audience\": \"www.google.com\", \"token_type\": "
              "\"LIMITED_AWS\"}");

    http_context.result = SuccessExecutionResult();
    http_context.response = make_shared<HttpResponse>();
    http_context.response->body = BytesBuffer("");
    http_context.Finish();
    return SuccessExecutionResult();
  });
  fetch_tee_token_context_.request = make_shared<GetTeeSessionTokenRequest>();
  fetch_tee_token_context_.request->token_target_audience_uri =
      make_shared<string>(kAudience);
  fetch_tee_token_context_.request->token_type =
      make_shared<string>(kTokenType);

  fetch_tee_token_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN)));
    finished_ = true;
  };

  authorizer_provider_->GetTeeSessionToken(fetch_tee_token_context_);
  WaitUntil([this]() { return finished_.load(); });
}

TEST(TokenTest, TokenExpire) {
  GetSessionTokenResponse response;
  response.expire_time =
      std::chrono::seconds(TimeUtil::GetCurrentTime().seconds());
  EXPECT_TRUE(TokenIsExpired(response));

  response.expire_time =
      std::chrono::seconds(TimeUtil::GetCurrentTime().seconds() + 305);
  EXPECT_FALSE(TokenIsExpired(response));

  response.expire_time =
      std::chrono::seconds(TimeUtil::GetCurrentTime().seconds() + 295);
  EXPECT_TRUE(TokenIsExpired(response));
}
}  // namespace google::scp::cpio::client_providers::test
