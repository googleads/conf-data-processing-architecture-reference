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

#ifndef SCP_CPIO_INTERFACE_CRYPTO_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_CRYPTO_CLIENT_TYPE_DEF_H_

#include <memory>
#include <string>

#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

namespace google::scp::cpio {

/// Configurations for CryptoClient.
struct CryptoClientOptions {
  virtual ~CryptoClientOptions() = default;
};

// Request for aead decryption using streaming API
struct AeadDecryptStreamRequest {
  virtual ~AeadDecryptStreamRequest() = default;

  // Passed in encrypted ciphertex stream.
  std::unique_ptr<std::istream> ciphertext_stream;
  // Streaming aead parameters
  cmrt::sdk::crypto_service::v1::StreamingAeadParams saead_params;
};

// Request for aead encryption using streaming API
struct AeadEncryptStreamRequest {
  virtual ~AeadEncryptStreamRequest() = default;

  // Destination stream where the encrypted data will be written to.
  std::unique_ptr<std::ostream> ciphertext_stream;
  // Streaming aead parameters
  cmrt::sdk::crypto_service::v1::StreamingAeadParams saead_params;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_CRYPTO_CLIENT_TYPE_DEF_H_
