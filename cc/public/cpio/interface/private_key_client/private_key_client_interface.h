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

#ifndef SCP_CPIO_INTERFACE_PRIVATE_KEY_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_PRIVATE_KEY_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for fetching private key from Key Management
 * Service.
 *
 * Use PrivateKeyClientFactory::Create to create the PrivateKeyClient. Call
 * PrivateKeyClientInterface::Init and PrivateKeyClientInterface::Run before
 * actually use it, and call PrivateKeyClientInterface::Stop when finish using
 * it.
 */
class PrivateKeyClientInterface : public core::ServiceInterface {
 public:
  virtual ~PrivateKeyClientInterface() = default;

  /**
   * @brief Lists a list of private keys for the given list of IDs or maximum
   * age. The private key is already decrypted by using KMS and can be used to
   * decrypt the payload directly.
   *
   * @param context the context of the operation.
   */
  virtual void ListPrivateKeys(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          context) noexcept = 0;

  /**
   * @brief Lists a list of private keys for the given list of IDs or maximum
   * age in a blocking call.
   *
   * @param request request to list private keys.
   * @return core::ExecutionResultOr<ListPrivateKeysResponse> result of the
   * operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>
  ListPrivateKeysSync(cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest
                          request) noexcept = 0;
};

/// Factory to create PrivateKeyClient.
class PrivateKeyClientFactory {
 public:
  /**
   * @brief Creates PrivateKeyClient.
   *
   * @param options configurations for PrivateKeyClient.
   * @return std::unique_ptr<PrivateKeyClientInterface> PrivateKeyClient object.
   */
  static std::unique_ptr<PrivateKeyClientInterface> Create(
      PrivateKeyClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_PRIVATE_KEY_CLIENT_INTERFACE_H_
