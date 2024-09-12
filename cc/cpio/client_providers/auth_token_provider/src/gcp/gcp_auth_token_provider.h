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

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/http_client_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AuthTokenProviderInterface
 */
class GcpAuthTokenProvider : public AuthTokenProviderInterface {
 public:
  GcpAuthTokenProvider(
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  void GetSessionToken(
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_token_context) noexcept override;

  void GetSessionTokenForTargetAudience(
      core::AsyncContext<GetSessionTokenForTargetAudienceRequest,
                         GetSessionTokenResponse>& get_token_context) noexcept
      override;

  void GetTeeSessionToken(
      core::AsyncContext<GetTeeSessionTokenRequest, GetSessionTokenResponse>&
          get_token_context) noexcept override;

 private:
  void GetSessionTokenInternal(
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_role_credentials_context) noexcept;

  void GetSessionTokenForTargetAudienceInternal(
      core::AsyncContext<GetSessionTokenForTargetAudienceRequest,
                         GetSessionTokenResponse>& get_token_context) noexcept;

  /**
   * @brief Is called when the get session token from current instance operation
   * is completed.
   *
   * @param get_token_context The context of the get session token
   * operation.
   * @param http_client_context http client operation context.
   */
  void OnGetSessionTokenCallback(
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_token_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  /**
   * @brief Is called when the get session token for target audience operation
   * is completed.
   *
   * @param get_token_context The context of the get session token
   * operation.
   * @param http_client_context http client operation context.
   */
  void OnGetSessionTokenForTargetAudienceCallback(
      core::AsyncContext<GetSessionTokenForTargetAudienceRequest,
                         GetSessionTokenResponse>& get_token_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  void OnGetTeeSessionTokenCallback(
      core::AsyncContext<GetTeeSessionTokenRequest, GetSessionTokenResponse>&
          get_token_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  /// HttpClient for issuing HTTP actions.
  std::shared_ptr<core::HttpClientInterface> http_client_;

  /// Token cached.
  GetSessionTokenResponse cached_token_;
  /// Mutex to protect the cached token.
  std::shared_mutex mutex_;

  /// Cached token for target audience.
  core::common::ConcurrentMap<std::string, GetSessionTokenResponse>
      cached_token_for_target_audience_;

  /// Operation distpatcher for retry.
  core::common::OperationDispatcher operation_dispatcher_;
};
}  // namespace google::scp::cpio::client_providers
