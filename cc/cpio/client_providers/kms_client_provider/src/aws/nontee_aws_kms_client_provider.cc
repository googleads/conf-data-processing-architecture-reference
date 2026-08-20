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

#include "nontee_aws_kms_client_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/client/AWSClient.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/kms/KMSClient.h>
#include <aws/kms/KMSErrors.h>
#include <aws/kms/model/DecryptRequest.h>

#include "absl/strings/str_cat.h"
#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "public/cpio/interface/kms_client/aws/type_def.h"
#include "public/cpio/interface/kms_client/type_def.h"

#include "aws_kms_client_provider_utils.h"
#include "nontee_error_codes.h"

namespace AwsKmsModel = Aws::KMS::Model;
namespace CmrtKmsProto = google::cmrt::sdk::kms_service::v1;
using Aws::Auth::AWSCredentials;
using Aws::Client::ClientConfiguration;
using Aws::KMS::KMSClient;
using crypto::tink::Aead;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_AUDIENCE_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_MISSING_COMPONENT;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::utils::Base64Decode;
using google::scp::cpio::common::CreateClientConfiguration;
using std::bind;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace {
/// Filename for logging errors
constexpr char kNonteeAwsKmsClientProvider[] = "NonteeAwsKmsClientProvider";
}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult NonteeAwsKmsClientProvider::Init() noexcept {
  if (!kms_client_options_) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_MISSING_COMPONENT);
    SCP_ERROR(kNonteeAwsKmsClientProvider, kZeroUuid, execution_result,
              "Null KmsClientOptions.");
    return execution_result;
  }

  if (!role_credentials_provider_) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_MISSING_COMPONENT);
    SCP_ERROR(kNonteeAwsKmsClientProvider, kZeroUuid, execution_result,
              "Null credential provider.");
    return execution_result;
  }

  if (!io_async_executor_) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_MISSING_COMPONENT);
    SCP_ERROR(kNonteeAwsKmsClientProvider, kZeroUuid, execution_result,
              "Null IO AsyncExecutor.");
    return execution_result;
  }

  if (!cpu_async_executor_) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_MISSING_COMPONENT);
    SCP_ERROR(kNonteeAwsKmsClientProvider, kZeroUuid, execution_result,
              "Null CPU AsyncExecutor.");
    return execution_result;
  }

  return aws_kms_service_client_cache_->Init();
}

ExecutionResult NonteeAwsKmsClientProvider::Run() noexcept {
  return aws_kms_service_client_cache_->Run();
}

ExecutionResult NonteeAwsKmsClientProvider::Stop() noexcept {
  return aws_kms_service_client_cache_->Stop();
}

void NonteeAwsKmsClientProvider::Decrypt(
    AsyncContext<CmrtKmsProto::DecryptRequest, DecryptResponse>&
        decrypt_context) noexcept {
  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  const auto& key_arn = decrypt_context.request->key_resource_name();
  if (key_arn.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get Key Arn from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  const auto& kms_region = decrypt_context.request->kms_region();
  if (kms_region.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get Key Region from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  const auto& account_identity = decrypt_context.request->account_identity();
  if (account_identity.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kNonteeAwsKmsClientProvider, decrypt_context, execution_result,
        "Failed to get Account Identity from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  const auto& key_ids = decrypt_context.request->key_ids();
  const auto& target_audience_for_web_identity =
      decrypt_context.request->target_audience_for_web_identity();
  // Key ids is valid only with audience.
  if (!key_ids.empty() && target_audience_for_web_identity.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_AUDIENCE_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get Audience from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  auto request = make_shared<GetRoleCredentialsRequest>();
  request->account_identity = make_shared<AccountIdentity>(account_identity);
  request->target_audience_for_web_identity = target_audience_for_web_identity;
  if (!key_ids.empty()) {
    request->key_ids =
        make_shared<vector<string>>(key_ids.begin(), key_ids.end());
  }
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_role_credentials_context(
          std::move(request),
          bind(&NonteeAwsKmsClientProvider::
                   GetSessionCredentialsCallbackToCreateKms,
               this, decrypt_context, _1),
          decrypt_context);
  role_credentials_provider_->GetRoleCredentials(get_role_credentials_context);
}

void NonteeAwsKmsClientProvider::GetSessionCredentialsCallbackToCreateKms(
    AsyncContext<CmrtKmsProto::DecryptRequest, DecryptResponse>&
        decrypt_context,
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_session_credentials_context) noexcept {
  auto execution_result = get_session_credentials_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider,
                      get_session_credentials_context, execution_result,
                      "Failed to get AWS Credentials.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  auto decoded_ciphertext_or =
      Base64Decode(decrypt_context.request->ciphertext());
  if (!decoded_ciphertext_or.result().Successful()) {
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      decoded_ciphertext_or.result(),
                      "Failed to decode ciphertext.");
    decrypt_context.result = decoded_ciphertext_or.result();
    decrypt_context.Finish();
    return;
  }

  AwsKmsModel::DecryptRequest decrypt_request;
  decrypt_request.SetKeyId(
      decrypt_context.request->key_resource_name().c_str());
  auto decoded_ciphertext = std::move(*decoded_ciphertext_or);
  Aws::Utils::ByteBuffer ciphertext_buffer(
      reinterpret_cast<const unsigned char*>(decoded_ciphertext.data()),
      decoded_ciphertext.length());
  decrypt_request.SetCiphertextBlob(ciphertext_buffer);

  auto schedule_result = io_async_executor_->Schedule(
      bind(&NonteeAwsKmsClientProvider::DecryptInternal, this, decrypt_context,
           get_session_credentials_context, std::move(decrypt_request)),
      AsyncPriority::Normal);

  if (!schedule_result.Successful()) {
    decrypt_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      schedule_result, "Failed to schedule Aws KMS Decrypt().");
    decrypt_context.Finish();
  }
}

void NonteeAwsKmsClientProvider::DecryptInternal(
    AsyncContext<CmrtKmsProto::DecryptRequest, DecryptResponse>&
        decrypt_context,
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_session_credentials_context,
    AwsKmsModel::DecryptRequest& decrypt_request) noexcept {
  const GetRoleCredentialsResponse& response =
      *get_session_credentials_context.response;
  AWSCredentials aws_credentials(response.access_key_id->c_str(),
                                 response.access_key_secret->c_str(),
                                 response.security_token->c_str());
  auto kms_client = GetOrCreateKmsClient(aws_credentials,
                                         decrypt_context.request->kms_region());

  auto decrypt_outcome = kms_client->Decrypt(decrypt_request);
  if (!decrypt_outcome.IsSuccess()) {
    decrypt_context.result =
        AwsKmsClientUtils::ConvertKmsError(decrypt_outcome.GetError());

    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      decrypt_context.result,
                      "KMS decrypt failed for key ID: %s",
                      decrypt_context.request->key_resource_name().c_str());
    FinishContext(decrypt_context.result, decrypt_context, cpu_async_executor_,
                  AsyncPriority::High);
    return;
  }
  if (decrypt_outcome.GetResult().GetKeyId() !=
      Aws::String(decrypt_context.request->key_resource_name().c_str())) {
    decrypt_context.result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED);
    SCP_ERROR_CONTEXT(
        kNonteeAwsKmsClientProvider, decrypt_context, decrypt_context.result,
        "AWS KMS decryption failed: wrong key ARN. Expected is: %s",
        decrypt_context.request->key_resource_name().c_str());
    FinishContext(decrypt_context.result, decrypt_context, cpu_async_executor_,
                  AsyncPriority::High);
    return;
  }

  auto& buffer = decrypt_outcome.GetResult().GetPlaintext();
  std::string plaintext(
      reinterpret_cast<const char*>(buffer.GetUnderlyingData()),
      buffer.GetLength());

  decrypt_context.response = make_shared<DecryptResponse>();
  decrypt_context.response->set_plaintext(std::move(plaintext));

  FinishContext(SuccessExecutionResult(), decrypt_context, cpu_async_executor_,
                AsyncPriority::High);
}

shared_ptr<KMSClient> NonteeAwsKmsClientProvider::GetOrCreateKmsClient(
    const AWSCredentials& aws_credentials, const string& kms_region) noexcept {
  shared_ptr<KMSClient> client_found;
  string cache_key =
      absl::StrCat(aws_credentials.GetAWSAccessKeyId(), ":",
                   aws_credentials.GetAWSSecretKey(), ":",
                   aws_credentials.GetSessionToken(), ":", kms_region);

  if (kms_client_options_->enable_aws_kms_client_cache) {
    if (!kms_region.empty() &&
        aws_kms_service_client_cache_->Find(cache_key, client_found)
            .Successful()) {
      return client_found;
    }
  }

  auto client = GetKmsClient(aws_credentials, kms_region);
  if (!kms_client_options_->enable_aws_kms_client_cache || kms_region.empty()) {
    return client;
  }

  std::pair<string, shared_ptr<KMSClient>> client_pair;
  client_pair.first = cache_key;
  client_pair.second = client;
  // Ignore insert error
  aws_kms_service_client_cache_->Insert(client_pair, client_found);
  return client_found;
}

shared_ptr<ClientConfiguration>
NonteeAwsKmsClientProvider::CreateClientConfiguration(
    const string& region) noexcept {
  auto client_config =
      common::CreateClientConfiguration(make_shared<string>(region));
  client_config->executor = make_shared<AwsAsyncExecutor>(io_async_executor_);
  return client_config;
}

shared_ptr<KMSClient> NonteeAwsKmsClientProvider::GetKmsClient(
    const AWSCredentials& aws_credentials, const string& kms_region) noexcept {
  return make_shared<KMSClient>(aws_credentials,
                                *CreateClientConfiguration(kms_region));
}
}  // namespace google::scp::cpio::client_providers
