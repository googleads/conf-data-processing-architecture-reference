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

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/crypto_client/crypto_client_interface.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/interface/type_def.h"

using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeAead;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeKdf;
using google::cmrt::sdk::crypto_service::v1::HpkeKem;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::CryptoClientFactory;
using google::scp::cpio::CryptoClientInterface;
using google::scp::cpio::CryptoClientOptions;
using google::scp::cpio::LogOption;
using std::atomic;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::string;
using std::unique_ptr;
using std::placeholders::_1;
using std::placeholders::_2;

constexpr char kPublicKey[] = "testpublickey==";
constexpr char kPrivateKey[] = "testprivatekey=";
constexpr char kSharedInfo[] = "shared_info";
constexpr char kRequestPayload[] = "abcdefg";
constexpr char kResponsePayload[] = "hijklmn";

unique_ptr<CryptoClientInterface> crypto_client;

int main(int argc, char* argv[]) {
  bool is_bidirectional = false;
  if (argc > 1) {
    is_bidirectional = string(argv[1]) == "true";
  }

  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  CryptoClientOptions crypto_client_options;

  crypto_client = CryptoClientFactory::Create(move(crypto_client_options));
  result = crypto_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init crypto client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = crypto_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run crypto client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }

  std::cout << "Run crypto client successfully!" << std::endl;

  HpkeEncryptRequest hpke_encrypt_request;
  hpke_encrypt_request.mutable_raw_key_with_params()->set_raw_key(
      string(kPublicKey));
  hpke_encrypt_request.mutable_raw_key_with_params()
      ->mutable_hpke_params()
      ->set_kem(HpkeKem::DHKEM_X25519_HKDF_SHA256);
  hpke_encrypt_request.mutable_raw_key_with_params()
      ->mutable_hpke_params()
      ->set_kdf(HpkeKdf::HKDF_SHA256);
  hpke_encrypt_request.mutable_raw_key_with_params()
      ->mutable_hpke_params()
      ->set_aead(HpkeAead::CHACHA20_POLY1305);
  hpke_encrypt_request.set_shared_info(string(kSharedInfo));
  hpke_encrypt_request.set_payload(string(kRequestPayload));
  hpke_encrypt_request.set_is_bidirectional(is_bidirectional);
  auto hpke_encrypt_response_or =
      crypto_client->HpkeEncryptSync(hpke_encrypt_request);
  if (!hpke_encrypt_response_or.result().Successful()) {
    std::cout << "Cannot HpkeEncrypt!" << GetErrorMessage(result.status_code)
              << std::endl;
  } else {
    std::cout << "Hpke encrypt success!" << std::endl;
    HpkeDecryptRequest hpke_decrypt_request;
    hpke_decrypt_request.mutable_raw_key_with_params()->set_raw_key(
        kPrivateKey);
    hpke_decrypt_request.mutable_raw_key_with_params()
        ->mutable_hpke_params()
        ->set_kem(HpkeKem::DHKEM_X25519_HKDF_SHA256);
    hpke_decrypt_request.mutable_raw_key_with_params()
        ->mutable_hpke_params()
        ->set_kdf(HpkeKdf::HKDF_SHA256);
    hpke_decrypt_request.mutable_raw_key_with_params()
        ->mutable_hpke_params()
        ->set_aead(HpkeAead::CHACHA20_POLY1305);
    hpke_decrypt_request.set_shared_info(string(kSharedInfo));
    hpke_decrypt_request.set_is_bidirectional(is_bidirectional);
    hpke_decrypt_request.mutable_encrypted_data()->set_ciphertext(
        hpke_encrypt_response_or->encrypted_data().ciphertext());
    hpke_decrypt_request.mutable_encrypted_data()->set_key_id(
        hpke_encrypt_response_or->encrypted_data().key_id());
    auto hpke_decrypt_response_or =
        crypto_client->HpkeDecryptSync(hpke_decrypt_request);
    if (!hpke_decrypt_response_or.result().Successful()) {
      std::cout << "Cannot HpkeDecrypt!"
                << GetErrorMessage(
                       hpke_decrypt_response_or.result().status_code)
                << std::endl;
    } else {
      std::cout << "Hpke decrypt success! Decrypted request Payload: "
                << hpke_decrypt_response_or->payload() << std::endl;
      if (is_bidirectional) {
        std::cout << "Response payload to be encrypted using Aead: "
                  << kResponsePayload << std::endl;
        AeadEncryptRequest aead_encrypt_request;
        aead_encrypt_request.set_shared_info(string(kSharedInfo));
        aead_encrypt_request.set_payload(string(kResponsePayload));
        auto secret = hpke_decrypt_response_or->secret();
        aead_encrypt_request.set_secret(secret);
        auto aead_encrypt_response_or =
            crypto_client->AeadEncryptSync(aead_encrypt_request);
        if (!aead_encrypt_response_or.result().Successful()) {
          std::cout << "Cannot AeadEncrypt!"
                    << GetErrorMessage(
                           aead_encrypt_response_or.result().status_code)
                    << std::endl;
        } else {
          std::cout << "Aead encrypt success!" << std::endl;
          AeadDecryptRequest aead_decrypt_request;
          aead_decrypt_request.set_shared_info(string(kSharedInfo));
          aead_decrypt_request.set_secret(secret);
          aead_decrypt_request.mutable_encrypted_data()->set_ciphertext(
              aead_encrypt_response_or->encrypted_data().ciphertext());
          auto aead_decrypt_response_or =
              crypto_client->AeadDecryptSync(aead_decrypt_request);
          if (!aead_decrypt_response_or.result().Successful()) {
            std::cout << "Cannot AeadDecrypt!"
                      << GetErrorMessage(
                             aead_decrypt_response_or.result().status_code)
                      << std::endl;
          } else {
            std::cout << "Aead decrypt success! Decrytped response payload: "
                      << aead_decrypt_response_or->payload() << std::endl;
          }
        }
      }
    }
  }

  result = crypto_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop crypto client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  return 0;
}
