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

#ifndef SCP_CPIO_INTERFACE_CRYPTO_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_CRYPTO_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include <tink/input_stream.h>
#include <tink/output_stream.h>

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for encrypting and decrypting data.
 *
 * Use CryptoClientFactory::Create to create the CryptoClient. Call
 * CryptoClientInterface::Init and CryptoClientInterface::Run before
 * actually use it, and call CryptoClientInterface::Stop when finish using
 * it.
 */
class CryptoClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Encypts payload using HPKE in a blocking call.
   *
   * @param request request to HPKE encrypt.
   * @return ExecutionResultOr<HpkeEncryptResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::crypto_service::v1::HpkeEncryptResponse>
  HpkeEncryptSync(const cmrt::sdk::crypto_service::v1::HpkeEncryptRequest&
                      request) noexcept = 0;

  /**
   * @brief Decrypts payload using HPKE in a blocking call.
   *
   * @param request request to HPKE decrypt.
   * @return ExecutionResultOr<HpkeDecryptResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>
  HpkeDecryptSync(const cmrt::sdk::crypto_service::v1::HpkeDecryptRequest&
                      request) noexcept = 0;

  /**
   * @brief Encrypts payload using Aead in a blocking call.
   *
   * @param request request to AEAD encrypt.
   * @return ExecutionResultOr<AeadEncryptResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::crypto_service::v1::AeadEncryptResponse>
  AeadEncryptSync(const cmrt::sdk::crypto_service::v1::AeadEncryptRequest&
                      request) noexcept = 0;

  /**
   * @brief Decrypts payload using Aead in a blocking call.
   *
   * @param request request to AEAD decrypt.
   * @return ExecutionResultOr<AeadDecryptResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::crypto_service::v1::AeadDecryptResponse>
  AeadDecryptSync(const cmrt::sdk::crypto_service::v1::AeadDecryptRequest&
                      request) noexcept = 0;

  /**
   * @brief Encrypts payload using Aead in a blocking call using streaming
   * manner. A wrapper around the ciphertext output stream will be returned to
   * push and encrypt plaintext to the output stream.
   *
   * @param request request to AEAD encrypt.
   * @return ExecutionResultOr<std::unique_ptr<::crypto::tink::OutputStream>> A
   * wrapper around the ciphertext output stream used to encrypt and push
   * plaintext.
   */
  virtual core::ExecutionResultOr<std::unique_ptr<::crypto::tink::OutputStream>>
  AeadEncryptStreamSync(
      const google::scp::cpio::AeadEncryptStreamRequest& request) noexcept = 0;

  /**
   * @brief Decrypts payload using Aead in a blocking call using streaming
   * manner. A wrapper around the ciphertext input stream will be returned for
   * decrypting ciphertext into plaintext.
   *
   * @param request request to AEAD decrypt.
   * @return ExecutionResultOr<std::unique_ptr<::crypto::tink::InputStream>> A
   * wrapper around the ciphertext input stream used to decrypt ciphertext into
   * plaintext.
   */
  virtual core::ExecutionResultOr<std::unique_ptr<::crypto::tink::InputStream>>
  AeadDecryptStreamSync(
      const google::scp::cpio::AeadDecryptStreamRequest& request) noexcept = 0;

  /**
   * @brief Computes Message Authentication Code in a blocking call.
   *
   * @param request request to compute Message Authentication Code.
   * @return ExecutionResultOr<ComputeMacResponse> result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::crypto_service::v1::ComputeMacResponse>
  ComputeMacSync(const cmrt::sdk::crypto_service::v1::ComputeMacRequest&
                     request) noexcept = 0;
};

/// Factory to create CryptoClient.
class CryptoClientFactory {
 public:
  /**
   * @brief Creates CryptoClient.
   *
   * @param options configurations for CryptoClient.
   * @return std::unique_ptr<CryptoClientInterface> CryptoClient object.
   */
  static std::unique_ptr<CryptoClientInterface> Create(
      CryptoClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_CRYPTO_CLIENT_INTERFACE_H_
