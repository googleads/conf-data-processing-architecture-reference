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

#include "gcp_kms_client_provider.h"

#include <memory>
#include <utility>

#include "core/utils/src/base64.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "google/cloud/kms/key_management_client.h"
#include "public/cpio/interface/kms_client/type_def.h"

#include "error_codes.h"
#include "gcp_key_management_service_client.h"

using google::cloud::kms::KeyManagementServiceClient;
using google::cloud::kms::MakeKeyManagementServiceConnection;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FinishContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::utils::Base64Decode;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;

/// Filename for logging errors
static constexpr char kGcpKmsClientProvider[] = "GcpKmsClientProvider";

namespace google::scp::cpio::client_providers {

ExecutionResult GcpKmsClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpKmsClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpKmsClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

void GcpKmsClientProvider::Decrypt(
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context) noexcept {
  if (decrypt_context.request->ciphertext().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  if (decrypt_context.request->key_resource_name().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kGcpKmsClientProvider, decrypt_context, execution_result,
        "Failed to get Key resource name from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpKmsClientProvider::AeadDecrypt, this, decrypt_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    decrypt_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context,
                      decrypt_context.result,
                      "AEAD decrypt failed to be scheduled");
    decrypt_context.Finish();
  }
}

void GcpKmsClientProvider::AeadDecrypt(
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context) noexcept {
  auto decoded_ciphertext_or =
      Base64Decode(decrypt_context.request->ciphertext());
  if (!decoded_ciphertext_or.Successful()) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Failed to decode the ciphertext using base64.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }
  auto gcp_kms = gcp_kms_factory_->CreateGcpKeyManagementServiceClient(
      decrypt_context.request->gcp_wip_provider(),
      decrypt_context.request->account_identity());

  string decoded_ciphertext = decoded_ciphertext_or.release();
  cloud::kms::v1::DecryptRequest req;
  req.set_name(decrypt_context.request->key_resource_name());
  req.set_ciphertext(decoded_ciphertext);

  auto response_or = gcp_kms->Decrypt(req);
  if (!response_or) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Decryption failed with error %s.",
                      response_or.status().message().c_str());
    FinishContext(execution_result, decrypt_context, cpu_async_executor_);
    return;
  }
  decrypt_context.response = make_shared<DecryptResponse>();
  decrypt_context.response->set_plaintext(move(response_or->plaintext()));

  FinishContext(SuccessExecutionResult(), decrypt_context, cpu_async_executor_);
}

shared_ptr<GcpKeyManagementServiceClientInterface>
GcpKmsFactory::CreateGcpKeyManagementServiceClient(
    const string& wip_provider,
    const string& service_account_to_impersonate) noexcept {
  auto key_management_service_client = CreateKeyManagementServiceClient(
      wip_provider, service_account_to_impersonate);
  return make_shared<GcpKeyManagementServiceClient>(
      key_management_service_client);
}

#ifndef TEST_CPIO
shared_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    const shared_ptr<KmsClientOptions>& options,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor) noexcept {
  return make_shared<GcpKmsClientProvider>(io_async_executor,
                                           cpu_async_executor);
}
#endif
}  // namespace google::scp::cpio::client_providers
