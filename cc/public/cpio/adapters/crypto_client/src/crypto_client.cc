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

#include "crypto_client.h"

#include <functional>
#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/errors.h"
#include "core/utils/src/error_utils.h"
#include "cpio/client_providers/crypto_client_provider/src/crypto_client_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/common/adapter_utils.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

using crypto::tink::InputStream;
using crypto::tink::OutputStream;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::ComputeMacRequest;
using google::cmrt::sdk::crypto_service::v1::ComputeMacResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::CryptoClientProvider;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using std::placeholders::_1;

namespace {
constexpr char kCryptoClient[] = "CryptoClient";
}

namespace google::scp::cpio {
CryptoClient::CryptoClient(const std::shared_ptr<CryptoClientOptions>& options)
    : options_(options) {
  crypto_client_provider_ = make_shared<CryptoClientProvider>(options_);
}

ExecutionResult CryptoClient::Init() noexcept {
  return ConvertToPublicExecutionResult(crypto_client_provider_->Init());
}

ExecutionResult CryptoClient::Run() noexcept {
  return ConvertToPublicExecutionResult(crypto_client_provider_->Run());
}

ExecutionResult CryptoClient::Stop() noexcept {
  return ConvertToPublicExecutionResult(crypto_client_provider_->Stop());
}

ExecutionResultOr<HpkeEncryptResponse> CryptoClient::HpkeEncryptSync(
    const HpkeEncryptRequest& request) noexcept {
  return ConvertToPublicExecutionResult(
      crypto_client_provider_->HpkeEncryptSync(request));
}

ExecutionResultOr<HpkeDecryptResponse> CryptoClient::HpkeDecryptSync(
    const HpkeDecryptRequest& request) noexcept {
  return ConvertToPublicExecutionResult(
      crypto_client_provider_->HpkeDecryptSync(request));
}

ExecutionResultOr<AeadEncryptResponse> CryptoClient::AeadEncryptSync(
    const AeadEncryptRequest& request) noexcept {
  return ConvertToPublicExecutionResult(
      crypto_client_provider_->AeadEncryptSync(request));
}

ExecutionResultOr<AeadDecryptResponse> CryptoClient::AeadDecryptSync(
    const AeadDecryptRequest& request) noexcept {
  return ConvertToPublicExecutionResult(
      crypto_client_provider_->AeadDecryptSync(request));
}

ExecutionResultOr<ComputeMacResponse> CryptoClient::ComputeMacSync(
    const ComputeMacRequest& request) noexcept {
  return ConvertToPublicExecutionResult(
      crypto_client_provider_->ComputeMacSync(request));
}

ExecutionResultOr<unique_ptr<InputStream>> CryptoClient::AeadDecryptStreamSync(
    const google::scp::cpio::AeadDecryptStreamRequest& request) noexcept {
  return crypto_client_provider_->AeadDecryptStreamSync(request);
};

ExecutionResultOr<unique_ptr<OutputStream>> CryptoClient::AeadEncryptStreamSync(
    const google::scp::cpio::AeadEncryptStreamRequest& request) noexcept {
  return crypto_client_provider_->AeadEncryptStreamSync(request);
}

std::unique_ptr<CryptoClientInterface> CryptoClientFactory::Create(
    CryptoClientOptions options) {
  return make_unique<CryptoClient>(make_shared<CryptoClientOptions>(options));
}
}  // namespace google::scp::cpio
