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

#include "gcp_auth_token_provider.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/type_def.h"
#include "core/utils/src/base64.h"

#include "error_codes.h"

using absl::StrFormat;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
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
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::RetryStrategy;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::errors::
    SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN;
using google::scp::core::errors::
    SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::utils::Base64Decode;
using google::scp::core::utils::PadBase64Encoding;
using nlohmann::json;
using std::all_of;
using std::array;
using std::bind;
using std::cbegin;
using std::cend;
using std::make_pair;
using std::make_shared;
using std::move;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_lock;
using std::vector;
using std::chrono::seconds;
using std::placeholders::_1;

namespace {
constexpr char kGcpAuthTokenProvider[] = "GcpAuthTokenProvider";

// This is not HTTPS but this is still safe according to the docs:
// https://cloud.google.com/compute/docs/metadata/overview#metadata_security_considerations
constexpr char kTokenServerPath[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/"
    "service-accounts/default/token";
constexpr char kIdentityServerPath[] =
    "http://metadata/computeMetadata/v1/instance/service-accounts/default/"
    "identity";
constexpr char kMetadataFlavorHeader[] = "Metadata-Flavor";
constexpr char kMetadataFlavorHeaderValue[] = "Google";
constexpr char kJsonAccessTokenKey[] = "access_token";
constexpr char kJsonTokenExpiryKey[] = "expires_in";
constexpr char kJsonTokenTypeKey[] = "token_type";
constexpr char kAudienceParameter[] = "audience=";
constexpr char kFormatFullParameter[] = "format=full";

constexpr size_t kExpectedTokenPartsSize = 3;
constexpr char kJsonTokenIssuerKey[] = "iss";
constexpr char kJsonTokenAudienceKey[] = "aud";
constexpr char kJsonTokenSubjectKey[] = "sub";
constexpr char kJsonTokenIssuedAtKey[] = "iat";
constexpr char kJsonTokenExpiryKeyForTargetAudience[] = "exp";
// Refetch the token kTokenExpireGracePeriodInSeconds before it expires.
constexpr int16_t kTokenExpireGracePeriodInSeconds = 300;

constexpr char kTeeTokenServerPath[] = "http://localhost/v1/token";
constexpr char kTeeTokenUnixSocketPath[] =
    "/run/container_launcher/teeserver.sock";
constexpr char kTeeTokenRequestBody[] =
    "{\"audience\": \"%s\", \"token_type\": \"%s\"}";
constexpr char kContentTypeHeaderKey[] = "Content-Type";
constexpr char kJsonContentTypeHeaderValue[] = "application/json";

constexpr int kGetAuthTokenRetryStrategyDelayInMs = 51;
constexpr int kGetAuthTokenRetryStrategyMaxRetries = 5;

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredJWTComponents() {
  static char const* components[3];
  using iterator_type = decltype(cbegin(components));
  static pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kJsonAccessTokenKey;
    components[1] = kJsonTokenExpiryKey;
    components[2] = kJsonTokenTypeKey;
    return make_pair(cbegin(components), cend(components));
  }();
  return iterator_pair;
}

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredJWTComponentsForTargetAudienceToken() {
  static char const* components[5];
  using iterator_type = decltype(cbegin(components));
  static pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kJsonTokenIssuerKey;
    components[1] = kJsonTokenAudienceKey;
    components[2] = kJsonTokenSubjectKey;
    components[3] = kJsonTokenIssuedAtKey;
    components[4] = kJsonTokenExpiryKeyForTargetAudience;
    return make_pair(cbegin(components), cend(components));
  }();
  return iterator_pair;
}

/// Indicates whether the token is expired.
bool TokenIsExpired(
    const google::scp::cpio::client_providers::GetSessionTokenResponse&
        token_reponse) {
  return token_reponse.expire_time.count() <
         TimeUtil::GetCurrentTime().seconds() -
             kTokenExpireGracePeriodInSeconds;
}
}  // namespace

namespace google::scp::cpio::client_providers {
GcpAuthTokenProvider::GcpAuthTokenProvider(
    const shared_ptr<HttpClientInterface>& http_client,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor)
    : http_client_(http_client),
      operation_dispatcher_(io_async_executor,
                            RetryStrategy(RetryStrategyOptions{
                                RetryStrategyType::Exponential,
                                kGetAuthTokenRetryStrategyDelayInMs,
                                kGetAuthTokenRetryStrategyMaxRetries})) {}

ExecutionResult GcpAuthTokenProvider::Init() noexcept {
  if (!http_client_) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED);
    SCP_ERROR(kGcpAuthTokenProvider, kZeroUuid, execution_result,
              "Http client cannot be nullptr.");
    return execution_result;
  }

  return SuccessExecutionResult();
};

ExecutionResult GcpAuthTokenProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpAuthTokenProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

void GcpAuthTokenProvider::GetSessionToken(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  operation_dispatcher_
      .Dispatch<AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>>(
          get_token_context,
          [this](AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
                     context) mutable {
            unique_lock lock(mutex_);
            if (!TokenIsExpired(cached_token_)) {
              SCP_DEBUG(kGcpAuthTokenProvider, kZeroUuid, "Found token cache.");
              context.response =
                  make_shared<GetSessionTokenResponse>(cached_token_);
              context.result = SuccessExecutionResult();
              context.Finish();
              return SuccessExecutionResult();
            }
            lock.unlock();
            GetSessionTokenInternal(context);
            return SuccessExecutionResult();
          });
}

void GcpAuthTokenProvider::GetSessionTokenForTargetAudience(
    AsyncContext<GetSessionTokenForTargetAudienceRequest,
                 GetSessionTokenResponse>& get_token_context) noexcept {
  operation_dispatcher_.Dispatch<AsyncContext<
      GetSessionTokenForTargetAudienceRequest, GetSessionTokenResponse>>(
      get_token_context,
      [this](AsyncContext<GetSessionTokenForTargetAudienceRequest,
                          GetSessionTokenResponse>& context) mutable {
        GetSessionTokenResponse token_response;
        auto result = cached_token_for_target_audience_.Find(
            *context.request->token_target_audience_uri, token_response);
        if (result.Successful() && !TokenIsExpired(token_response)) {
          SCP_DEBUG(kGcpAuthTokenProvider, kZeroUuid,
                    "Found token cache for target audience.");
          context.response =
              make_shared<GetSessionTokenResponse>(token_response);
          context.result = SuccessExecutionResult();
          context.Finish();
          return SuccessExecutionResult();
        }
        GetSessionTokenForTargetAudienceInternal(context);
        return SuccessExecutionResult();
      });
}

void GcpAuthTokenProvider::GetSessionTokenInternal(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  // Make a request to the metadata server:
  // The Application is running on a GCP VM which runs as a service account.
  // Services which run on GCP also spin up a local metadata server which can be
  // queried for details about the system. curl -H "Metadata-Flavor: Google"
  // 'http://metadata.google.internal/computeMetadata/v1/instance/service-accounts/default/token?scopes=SCOPES'
  // NOTE: Without scope setting, the access token will being assigned with full
  // access permission of current instance.
  auto request = make_shared<HttpRequest>();
  request->headers = make_shared<HttpHeaders>();
  request->headers->insert(
      {string(kMetadataFlavorHeader), string(kMetadataFlavorHeaderValue)});
  request->path = make_shared<Uri>(kTokenServerPath);

  AsyncContext<HttpRequest, HttpResponse> http_context(
      std::move(request),
      bind(&GcpAuthTokenProvider::OnGetSessionTokenCallback, this,
           get_token_context, _1),
      get_token_context);

  auto execution_result = http_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context,
                      execution_result,
                      "Failed to perform http request to fetch access token.");

    get_token_context.result = execution_result;
    get_token_context.Finish();
  }
}

void GcpAuthTokenProvider::OnGetSessionTokenCallback(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, http_client_context.result,
        "Failed to get access token from Instance Metadata server");

    get_token_context.result = http_client_context.result;
    get_token_context.Finish();
    return;
  }

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, result,
        "Received http response could not be parsed into a JSON.");
    get_token_context.result = result;
    get_token_context.Finish();
    return;
  }

  if (!all_of(GetRequiredJWTComponents().first,
              GetRequiredJWTComponents().second,
              [&json_response](const char* const component) {
                return json_response.contains(component);
              })) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, result,
        "Received http response does not contain all the necessary fields: %s",
        json_response.dump().c_str());
    get_token_context.result = result;
    get_token_context.Finish();
    return;
  }

  get_token_context.response = make_shared<GetSessionTokenResponse>();

  // The life time of GCP access token is about 1 hour.
  uint64_t expiry_seconds = json_response[kJsonTokenExpiryKey].get<uint64_t>();
  GetSessionTokenResponse response;
  response.expire_time =
      seconds(TimeUtil::GetCurrentTime().seconds() + expiry_seconds);
  auto access_token = json_response[kJsonAccessTokenKey].get<string>();
  response.session_token = make_shared<string>(move(access_token));

  unique_lock lock(mutex_);
  cached_token_ = response;
  lock.unlock();

  get_token_context.response =
      make_shared<GetSessionTokenResponse>(std::move(response));
  get_token_context.result = SuccessExecutionResult();
  get_token_context.Finish();
}

void GcpAuthTokenProvider::GetSessionTokenForTargetAudienceInternal(
    AsyncContext<GetSessionTokenForTargetAudienceRequest,
                 GetSessionTokenResponse>& get_token_context) noexcept {
  // Make a request to the metadata server:
  // The PBS is running on a GCP VM which runs as a service account. Services
  // which run on GCP also spin up a local metadata server which can be
  // queried for details about the system. curl -H "Metadata-Flavor: Google"
  // 'http://metadata/computeMetadata/v1/instance/service-accounts/default/identity?audience=AUDIENCE'

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = make_shared<HttpRequest>();

  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {string(kMetadataFlavorHeader), string(kMetadataFlavorHeaderValue)});

  http_context.request->path = make_shared<Uri>(kIdentityServerPath);
  http_context.request->query = make_shared<string>(absl::StrCat(
      kAudienceParameter, *get_token_context.request->token_target_audience_uri,
      "&", kFormatFullParameter));

  http_context.callback =
      bind(&GcpAuthTokenProvider::OnGetSessionTokenForTargetAudienceCallback,
           this, get_token_context, _1);

  auto result = http_client_->PerformRequest(http_context);
  if (!result.Successful()) {
    get_token_context.result = result;
    get_token_context.Finish();
  }
}

void GcpAuthTokenProvider::OnGetSessionTokenForTargetAudienceCallback(
    AsyncContext<GetSessionTokenForTargetAudienceRequest,
                 GetSessionTokenResponse>& get_token_context,
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  get_token_context.result = http_context.result;
  if (!http_context.result.Successful()) {
    get_token_context.Finish();
    return;
  }
  const auto& response_body = http_context.response->body.ToString();
  vector<string> token_parts = absl::StrSplit(response_body, '.');
  if (token_parts.size() != kExpectedTokenPartsSize) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context, result,
                      "Received token does not have %d parts: %s",
                      kExpectedTokenPartsSize, response_body.c_str());
    get_token_context.result = result;
    get_token_context.Finish();
    return;
  }

  // The JSON Web Token (JWT) lives in the middle (1) part of the whole
  // string.
  auto padded_jwt_or = PadBase64Encoding(token_parts[1]);
  if (!padded_jwt_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, padded_jwt_or.result(),
        "Received JWT cannot be padded correctly: %s", response_body.c_str());
    get_token_context.result = padded_jwt_or.result();
    get_token_context.Finish();
    return;
  }
  auto decoded_json_str_or = Base64Decode(*padded_jwt_or);
  if (!decoded_json_str_or.Successful()) {
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context,
                      decoded_json_str_or.result(),
                      "Received token JWT could not be decoded.");
    get_token_context.result = decoded_json_str_or.result();
    get_token_context.Finish();
    return;
  }
  json json_web_token;
  try {
    json_web_token = json::parse(*decoded_json_str_or);
  } catch (...) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context, result,
                      "Received JWT could not be parsed into a JSON.");
    get_token_context.result = result;
    get_token_context.Finish();
    return;
  }

  if (!std::all_of(GetRequiredJWTComponentsForTargetAudienceToken().first,
                   GetRequiredJWTComponentsForTargetAudienceToken().second,
                   [&json_web_token](const char* const component) {
                     return json_web_token.contains(component);
                   })) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, result,
        "Received JWT does not contain all the necessary fields.");
    get_token_context.result = result;
    get_token_context.Finish();
    return;
  }

  GetSessionTokenResponse token_response;
  token_response.session_token = make_shared<string>(move(response_body));
  uint64_t expiry_seconds =
      json_web_token[kJsonTokenExpiryKeyForTargetAudience].get<uint64_t>();
  token_response.expire_time = seconds(expiry_seconds);
  auto cached_token_pair = make_pair(
      *get_token_context.request->token_target_audience_uri, token_response);
  // Need to erase the token first because ConcurrentMap::Insert doesn't
  // overwrite.
  auto result = cached_token_for_target_audience_.Erase(
      *get_token_context.request->token_target_audience_uri);
  // Don't treat it as error because it may happen if some other thread already
  // remove the cached token first.
  if (!result.Successful()) {
    SCP_DEBUG(
        kGcpAuthTokenProvider, kZeroUuid,
        "Failed to erase cached token for target audience. Target audience "
        "is: %s",
        get_token_context.request->token_target_audience_uri->c_str());
  }
  result = cached_token_for_target_audience_.Insert(cached_token_pair,
                                                    token_response);
  // Don't treat it as error because it may happen if some other thread already
  // insert the cached token first.
  if (!result.Successful()) {
    SCP_DEBUG(
        kGcpAuthTokenProvider, kZeroUuid,
        "Failed to insert cached token for target audience. Target audience "
        "is: %s",
        get_token_context.request->token_target_audience_uri->c_str());
  }
  get_token_context.response =
      make_shared<GetSessionTokenResponse>(move(token_response));
  get_token_context.result = SuccessExecutionResult();
  get_token_context.Finish();
}

void GcpAuthTokenProvider::GetTeeSessionToken(
    AsyncContext<GetTeeSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  auto request = make_shared<HttpRequest>();
  request->method = HttpMethod::POST;
  request->path = make_shared<Uri>(kTeeTokenServerPath);
  request->unix_socket_path = make_shared<Uri>(kTeeTokenUnixSocketPath);
  request->headers = make_shared<HttpHeaders>();
  request->headers->insert(
      {kContentTypeHeaderKey, kJsonContentTypeHeaderValue});
  string body_str =
      absl::StrFormat(kTeeTokenRequestBody,
                      *get_token_context.request->token_target_audience_uri,
                      *get_token_context.request->token_type);
  request->body = BytesBuffer(body_str);

  AsyncContext<HttpRequest, HttpResponse> http_context(
      std::move(request),
      bind(&GcpAuthTokenProvider::OnGetTeeSessionTokenCallback, this,
           get_token_context, _1),
      get_token_context);

  auto execution_result = http_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, execution_result,
        "Failed to perform http request to fetch TEE access token.");

    get_token_context.result = execution_result;
    get_token_context.Finish();
  }
}

void GcpAuthTokenProvider::OnGetTeeSessionTokenCallback(
    AsyncContext<GetTeeSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context,
                      http_client_context.result,
                      "Failed to get Tee access token.");

    get_token_context.result = http_client_context.result;
    get_token_context.Finish();
    return;
  }
  auto token = http_client_context.response->body.ToString();
  if (token.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context,
                      execution_result, "Empty token.");

    get_token_context.result = execution_result;
    get_token_context.Finish();
    return;
  }
  get_token_context.response = make_shared<GetSessionTokenResponse>();
  get_token_context.response->session_token = make_shared<string>(move(token));

  get_token_context.result = SuccessExecutionResult();
  get_token_context.Finish();
}

std::shared_ptr<AuthTokenProviderInterface> AuthTokenProviderFactory::Create(
    const std::shared_ptr<core::HttpClientInterface>& http1_client,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpAuthTokenProvider>(http1_client, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
