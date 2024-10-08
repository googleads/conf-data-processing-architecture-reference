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

#ifndef SCP_CPIO_INTERFACE_KMS_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_KMS_CLIENT_INTERFACE_H_

#include <memory>
#include <string>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/kms_service/v1/kms_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for interacting with Kms.
 *
 * Use KmsClientFactory::Create to create the KmsClient. Call
 * KmsClientInterface::Init and KmsClientInterface::Run before
 * actually using it, and call KmsClientInterface::Stop when finished
 * using it.
 */
class KmsClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Decrypts some data.
   *
   * @param decrypt_context The context for the operation.
   */
  virtual void Decrypt(
      core::AsyncContext<google::cmrt::sdk::kms_service::v1::DecryptRequest,
                         google::cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context) noexcept = 0;

  /**
   * @brief Decrypts some data in a blocking call.
   *
   * @param request the request to decrypt.
   * @return core::ExecutionResultOr<DecryptResponse> the
   * execution result of the operation.
   */
  virtual core::ExecutionResultOr<
      google::cmrt::sdk::kms_service::v1::DecryptResponse>
  DecryptSync(
      google::cmrt::sdk::kms_service::v1::DecryptRequest request) noexcept = 0;
};

/// Factory to create KmsClient.
class KmsClientFactory {
 public:
  /**
   * @brief Creates KmsClient.
   *
   * @return std::unique_ptr<KmsClient> KmsClient object.
   */
  static std::unique_ptr<KmsClientInterface> Create(KmsClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_KMS_CLIENT_INTERFACE_H_
