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

#include "crypto_client_provider.h"

#include <cctype>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>

#include <tink/aead.h>
#include <tink/binary_keyset_reader.h>
#include <tink/cleartext_keyset_handle.h>
#include <tink/hybrid/hpke_config.h>
#include <tink/hybrid/internal/hpke_context.h>
#include <tink/hybrid_config.h>
#include <tink/hybrid_decrypt.h>
#include <tink/hybrid_encrypt.h>
#include <tink/keyset_handle.h>
#include <tink/mac.h>
#include <tink/mac/mac_config.h>
#include <tink/streaming_aead.h>
#include <tink/streamingaead/aes_ctr_hmac_streaming_key_manager.h>
#include <tink/streamingaead/aes_gcm_hkdf_streaming_key_manager.h>
#include <tink/subtle/aes_ctr_hmac_streaming.h>
#include <tink/subtle/aes_gcm_boringssl.h>
#include <tink/subtle/aes_gcm_hkdf_streaming.h>
#include <tink/subtle/common_enums.h>
#include <tink/tink_config.h>
#include <tink/util/istream_input_stream.h>
#include <tink/util/ostream_output_stream.h>
#include <tink/util/secret_data.h>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/interface/type_def.h"
#include "proto/hpke.pb.h"
#include "proto/tink.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

#include "error_codes.h"

using absl::HexStringToBytes;
using crypto::tink::Aead;
using crypto::tink::AesCtrHmacStreamingKeyManager;
using crypto::tink::AesGcmHkdfStreamingKeyManager;
using crypto::tink::BinaryKeysetReader;
using crypto::tink::CleartextKeysetHandle;
using crypto::tink::HybridConfig;
using crypto::tink::HybridDecrypt;
using crypto::tink::HybridEncrypt;
using crypto::tink::InputStream;
using crypto::tink::KeysetHandle;
using crypto::tink::KeysetReader;
using crypto::tink::Mac;
using crypto::tink::MacConfig;
using crypto::tink::OutputStream;
using crypto::tink::StreamingAead;
using crypto::tink::TinkConfig;
using crypto::tink::internal::ConcatenatePayload;
using crypto::tink::internal::HpkeContext;
using crypto::tink::internal::SplitPayload;
using crypto::tink::subtle::AesGcmBoringSsl;
using crypto::tink::util::IstreamInputStream;
using crypto::tink::util::OstreamOutputStream;
using crypto::tink::util::SecretData;
using crypto::tink::util::SecretDataAsStringView;
using crypto::tink::util::SecretDataFromStringView;
using crypto::tink::util::StatusOr;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::ComputeMacRequest;
using google::cmrt::sdk::crypto_service::v1::ComputeMacResponse;
using google::cmrt::sdk::crypto_service::v1::HashType;
using google::cmrt::sdk::crypto_service::v1::HpkeAead;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeKdf;
using google::cmrt::sdk::crypto_service::v1::HpkeKem;
using google::cmrt::sdk::crypto_service::v1::HpkeParams;
using google::cmrt::sdk::crypto_service::v1::SecretLength;
using google::crypto::tink::AesCtrHmacStreamingKey;
using google::crypto::tink::AesGcmHkdfStreamingKey;
using google::crypto::tink::HpkePrivateKey;
using google::crypto::tink::HpkePublicKey;
using google::crypto::tink::KeyData;
using google::crypto::tink::Keyset;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::PublicPrivateKeyPairId;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_AEAD_DECRYPT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_AEAD_ENCRYPT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_CANNOT_COMPUTE_MAC;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_TINK_PRIMITIVE;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_READ_BINARY_KEY_SET;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_REGISTER_TINK_CONFIG;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_HPKE_DECRYPT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_HPKE_ENCRYPT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_INVALID_KEYSET_SIZE;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_MISSING_DATA;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_MISSING_KEY;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PRIVATE_KEY_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PUBLIC_KEY_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_PARSE_STREAMING_AEAD_KEY_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_READ_KEYSET_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_SAEAD_CONFIG_MISSING;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_SAEAD_CREATE_DECRYPT_STREAM_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_SAEAD_CREATE_ENCRYPT_STREAM_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_SECRET_EXPORT_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_SPLIT_CIPHERTEXT_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM;
using google::scp::core::utils::Base64Decode;
using std::bind;
using std::istream;
using std::isxdigit;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::mt19937;
using std::ostream;
using std::random_device;
using std::shared_ptr;
using std::string;
using std::uniform_int_distribution;
using std::unique_ptr;
using std::placeholders::_1;

namespace {
/// Filename for logging errors
constexpr char kCryptoClientProvider[] = "CryptoClientProvider";
constexpr char kDefaultExporterContext[] = "aead key";
}  // namespace

namespace google::scp::cpio::client_providers {
namespace tink = ::crypto::tink::internal;

/// Map from HpkeKem to Tink HpkeKem.
const map<HpkeKem, tink::HpkeKem> kHpkeKemMap = {
    {HpkeKem::DHKEM_X25519_HKDF_SHA256, tink::HpkeKem::kX25519HkdfSha256},
    {HpkeKem::DHKEM_P256_HKDF_SHA256, tink::HpkeKem::kP256HkdfSha256},
    {HpkeKem::KEM_UNKNOWN, tink::HpkeKem::kUnknownKem}};
/// Map from HpkeKdf to Tink HpkeKdf.
const map<HpkeKdf, tink::HpkeKdf> kHpkeKdfMap = {
    {HpkeKdf::HKDF_SHA256, tink::HpkeKdf::kHkdfSha256},
    {HpkeKdf::KDF_UNKNOWN, tink::HpkeKdf::kUnknownKdf}};
/// Map from HpkeAead to Tink HpkeAead.
const map<HpkeAead, tink::HpkeAead> kHpkeAeadMap = {
    {HpkeAead::AES_128_GCM, tink::HpkeAead::kAes128Gcm},
    {HpkeAead::AES_256_GCM, tink::HpkeAead::kAes256Gcm},
    {HpkeAead::CHACHA20_POLY1305, tink::HpkeAead::kChaCha20Poly1305},
    {HpkeAead::AEAD_UNKNOWN, tink::HpkeAead::kUnknownAead}};
/// Map from HashType to Tink EllipticCurveType
const map<HashType, google::crypto::tink::HashType> kHashTypeMap = {
    {HashType::UNKNOWN_HASH, google::crypto::tink::HashType::UNKNOWN_HASH},
    {HashType::SHA384, google::crypto::tink::HashType::SHA384},
    {HashType::SHA256, google::crypto::tink::HashType::SHA256},
    {HashType::SHA512, google::crypto::tink::HashType::SHA512},
    {HashType::SHA224, google::crypto::tink::HashType::SHA224},
    {HashType::SHA1, google::crypto::tink::HashType::SHA1}};
/// Map from Tink HpkeKem proto to Tink HpkeKem struct.
const map<crypto::tink::HpkeKem, tink::HpkeKem> kTinkInternalHpkeKemMap = {
    {crypto::tink::DHKEM_X25519_HKDF_SHA256, tink::HpkeKem::kX25519HkdfSha256},
    {crypto::tink::DHKEM_P256_HKDF_SHA256, tink::HpkeKem::kP256HkdfSha256},
    {crypto::tink::KEM_UNKNOWN, tink::HpkeKem::kUnknownKem}};
/// Map from Tink HpkeKdf proto to Tink HpkeKdf struct.
const map<crypto::tink::HpkeKdf, tink::HpkeKdf> kTinkInternalHpkeKdfMap = {
    {crypto::tink::HKDF_SHA256, tink::HpkeKdf::kHkdfSha256},
    {crypto::tink::KDF_UNKNOWN, tink::HpkeKdf::kUnknownKdf}};
/// Map from Tink HpkeAead proto to Tink HpkeAead struct.
const map<crypto::tink::HpkeAead, tink::HpkeAead> kTinkInternalHpkeAeadMap = {
    {crypto::tink::AES_128_GCM, tink::HpkeAead::kAes128Gcm},
    {crypto::tink::AES_256_GCM, tink::HpkeAead::kAes256Gcm},
    {crypto::tink::CHACHA20_POLY1305, tink::HpkeAead::kChaCha20Poly1305},
    {crypto::tink::AEAD_UNKNOWN, tink::HpkeAead::kUnknownAead}};

/**
 * @brief Converts HpkeParams we have to Tink' HpkeParams. hpke_params_proto
 * will override the default HpkeParams or what we've configured.
 *
 * @param hpke_params_proto HpkeParams from input request.
 * @return ExecutionResultOr<HpkeParams> HpkeParams to be used by Tink.
 */
ExecutionResultOr<tink::HpkeParams> ToTinkHpkeParamsFromScp(
    const HpkeParams& hpke_params_proto) {
  tink::HpkeParams hpke_params;

  if (kHpkeKemMap.find(hpke_params_proto.kem()) != kHpkeKemMap.end()) {
    hpke_params.kem = kHpkeKemMap.at(hpke_params_proto.kem());
  } else {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Invalid HPKE KEM.");
    return execution_result;
  }
  if (kHpkeKdfMap.find(hpke_params_proto.kdf()) != kHpkeKdfMap.end()) {
    hpke_params.kdf = kHpkeKdfMap.at(hpke_params_proto.kdf());
  } else {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Invalid HPKE KDF.");
    return execution_result;
  }
  if (kHpkeAeadMap.find(hpke_params_proto.aead()) != kHpkeAeadMap.end()) {
    hpke_params.aead = kHpkeAeadMap.at(hpke_params_proto.aead());
  } else {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Invalid HPKE AEAD.");
    return execution_result;
  }
  return hpke_params;
}

ExecutionResultOr<tink::HpkeParams> ToTinkHpkeParamsFromTinkProto(
    const crypto::tink::HpkeParams& tink_hpke_params_proto) {
  tink::HpkeParams hpke_params;

  if (kTinkInternalHpkeKemMap.find(tink_hpke_params_proto.kem()) !=
      kTinkInternalHpkeKemMap.end()) {
    hpke_params.kem = kTinkInternalHpkeKemMap.at(tink_hpke_params_proto.kem());
  } else {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Invalid HPKE KEM.");
    return execution_result;
  }
  if (kTinkInternalHpkeKdfMap.find(tink_hpke_params_proto.kdf()) !=
      kTinkInternalHpkeKdfMap.end()) {
    hpke_params.kdf = kTinkInternalHpkeKdfMap.at(tink_hpke_params_proto.kdf());
  } else {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Invalid HPKE KDF.");
    return execution_result;
  }
  if (kTinkInternalHpkeAeadMap.find(tink_hpke_params_proto.aead()) !=
      kTinkInternalHpkeAeadMap.end()) {
    hpke_params.aead =
        kTinkInternalHpkeAeadMap.at(tink_hpke_params_proto.aead());
  } else {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Invalid HPKE AEAD.");
    return execution_result;
  }
  return hpke_params;
}

/**
 * @brief Gets the Secret Length.
 *
 * @param secret_length SecretLength enum.
 * @return size_t the length.
 */
size_t GetSecretLength(const SecretLength& secret_length) {
  switch (secret_length) {
    case SecretLength::SECRET_LENGTH_32_BYTES:
      return 32;
    case SecretLength::SECRET_LENGTH_16_BYTES:
    default:
      return 16;
  }
}

/**
 * @brief Gets a random number between 0 and size-1.
 *
 * @param size specified size.
 * @return uint64_t the random number.
 */
uint64_t GetRandomNumber(int size) {
  static random_device random_device_local;
  static mt19937 random_generator(random_device_local());
  uniform_int_distribution<uint64_t> distribution;

  return distribution(random_generator) % size;
}

ExecutionResultOr<unique_ptr<KeysetReader>> CreateBinaryKeysetReader(
    const string& key) {
  auto keyset_reader_or = BinaryKeysetReader::New(key);
  if (!keyset_reader_or.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_READ_BINARY_KEY_SET);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Create Keyset reader failed with error. %s.",
              keyset_reader_or.status().ToString().c_str());
    return execution_result;
  }
  return std::move(*keyset_reader_or);
}

ExecutionResultOr<unique_ptr<Keyset>> CreateKeyset(const string& key) {
  auto keyset_reader_or = CreateBinaryKeysetReader(key);
  auto keyset_or = (*keyset_reader_or)->Read();
  if (!keyset_or.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_READ_KEYSET_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Read keyset failed with error %s.",
              keyset_or.status().ToString().c_str());
    return execution_result;
  }
  auto keyset = move(*keyset_or);
  if (keyset->key_size() != 1) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_INVALID_KEYSET_SIZE);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Invalid key size. Key size is %d.", keyset->key_size());
    return execution_result;
  }
  return keyset;
}

ExecutionResultOr<unique_ptr<KeysetHandle>> CreateKeysetHandle(
    const string& key) {
  auto decoded_key_or = Base64Decode(key);
  if (!decoded_key_or.Successful()) {
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, decoded_key_or.result(),
              "Decoding failed with error.");
    return decoded_key_or.result();
  }
  string decoded_key = decoded_key_or.release();

  auto keyset_reader_or = CreateBinaryKeysetReader(decoded_key);
  RETURN_IF_FAILURE(keyset_reader_or.result());

  auto keyset_handle = CleartextKeysetHandle::Read(move(*keyset_reader_or));
  if (!keyset_handle.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Creating Keyset failed with error %s.",
              keyset_handle.status().ToString().c_str());
    return execution_result;
  }
  return std::move(*keyset_handle);
}

ExecutionResult CryptoClientProvider::Init() noexcept {
  // Support to use Tink's primitives.
  auto register_result = HybridConfig::Register();
  if (!register_result.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_REGISTER_TINK_CONFIG);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Register Hybrid config with error %s.",
              register_result.ToString().c_str());
    return execution_result;
  }
  // Need to register HPKE explicitly.
  register_result = ::crypto::tink::RegisterHpke();
  if (!register_result.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_REGISTER_TINK_CONFIG);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Register HPKE config with error %s.",
              register_result.ToString().c_str());
    return execution_result;
  }
  register_result = MacConfig::Register();
  if (!register_result.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_REGISTER_TINK_CONFIG);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Register MAC config with error %s.",
              register_result.ToString().c_str());
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult CryptoClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult CryptoClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResultOr<HpkeEncryptResponse>
CryptoClientProvider::HpkeEncryptUsingExternalInterface(
    const HpkeEncryptRequest& encrypt_request) noexcept {
  auto keyset_handle_or = CreateKeysetHandle(encrypt_request.tink_key_binary());
  if (!keyset_handle_or.Successful()) {
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, keyset_handle_or.result(),
              "Creating KeysetHandle failed with error.");
    return keyset_handle_or.result();
  }

  auto primitive_or = (*keyset_handle_or)->GetPrimitive<HybridEncrypt>();
  if (!primitive_or.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_TINK_PRIMITIVE);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Creating Hpke Encrypt Primitive failed with error %s.",
              primitive_or.status().ToString().c_str());
    return execution_result;
  }

  auto encrypt_result_or =
      (*primitive_or)
          ->Encrypt(encrypt_request.payload(), encrypt_request.shared_info());
  if (!encrypt_result_or.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_HPKE_ENCRYPT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Hpke encryption failed with error %s.",
              encrypt_result_or.status().ToString().c_str());
    return execution_result;
  }

  HpkeEncryptResponse response;
  response.mutable_encrypted_data()->set_key_id(encrypt_request.key_id());
  response.mutable_encrypted_data()->set_ciphertext(
      std::move(*encrypt_result_or));
  return response;
}

ExecutionResultOr<HpkeEncryptResponse> CryptoClientProvider::HpkeEncryptSync(
    const HpkeEncryptRequest& encrypt_request) noexcept {
  if (!encrypt_request.has_raw_key_with_params() &&
      !encrypt_request.has_tink_key_binary()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_MISSING_KEY);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "HPKE encryption failed.");
    return execution_result;
  }

  // Use Tink's external interface for non bidirectional encryption when input
  // the tink_binary_key.
  if (!encrypt_request.is_bidirectional() &&
      encrypt_request.has_tink_key_binary()) {
    return HpkeEncryptUsingExternalInterface(encrypt_request);
  }

  string raw_key;
  tink::HpkeParams hpke_params;
  auto& encoded_key = encrypt_request.has_raw_key_with_params()
                          ? encrypt_request.raw_key_with_params().raw_key()
                          : encrypt_request.tink_key_binary();
  auto decoded_key_or = Base64Decode(encoded_key);
  if (!decoded_key_or.Successful()) {
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, decoded_key_or.result(),
              "HPKE encryption failed.");
    return decoded_key_or.result();
  }
  auto decoded_key = decoded_key_or.release();
  if (encrypt_request.has_raw_key_with_params()) {
    raw_key = decoded_key;
    auto hpke_params_or = ToTinkHpkeParamsFromScp(
        encrypt_request.raw_key_with_params().hpke_params());
    RETURN_AND_LOG_IF_FAILURE(hpke_params_or.result(), kCryptoClientProvider,
                              kZeroUuid, "Invalid HpkeParams");
    hpke_params = std::move(*hpke_params_or);
  } else {
    auto keyset_or = CreateKeyset(decoded_key);
    RETURN_IF_FAILURE(keyset_or.result());
    HpkePublicKey public_key;
    if (!public_key.ParseFromString(
            keyset_or.value()->key(0).key_data().value())) {
      auto execution_result = FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PUBLIC_KEY_FAILED);
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                "Failed to construct HpkePublicKey.");
      return execution_result;
    }
    raw_key = public_key.public_key();
    auto hpke_params_or = ToTinkHpkeParamsFromTinkProto(public_key.params());
    RETURN_AND_LOG_IF_FAILURE(hpke_params_or.result(), kCryptoClientProvider,
                              kZeroUuid, "Invalid HpkeParams");
    hpke_params = std::move(*hpke_params_or);
  }

  auto cipher = HpkeContext::SetupSender(hpke_params, raw_key,
                                         encrypt_request.shared_info());
  if (!cipher.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Hpke encryption failed with error %s.",
              cipher.status().ToString().c_str());
    return execution_result;
  }

  auto ciphertext =
      (*cipher)->Seal(encrypt_request.payload(), "" /*Empty associated data*/);
  if (!ciphertext.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_HPKE_ENCRYPT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Hpke encryption failed with error %s.",
              ciphertext.status().ToString().c_str());
    return execution_result;
  }

  HpkeEncryptResponse response;
  if (encrypt_request.is_bidirectional()) {
    auto secret =
        (*cipher)->Export(encrypt_request.exporter_context().empty()
                              ? kDefaultExporterContext
                              : encrypt_request.exporter_context(),
                          GetSecretLength(encrypt_request.secret_length()));
    if (!secret.ok()) {
      auto execution_result = FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_SECRET_EXPORT_FAILED);
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                "Hpke encryption failed with error %s.",
                secret.status().ToString().c_str());
      return execution_result;
    }
    response.set_secret(string(SecretDataAsStringView((*secret))));
  }

  response.mutable_encrypted_data()->set_key_id(encrypt_request.key_id());
  response.mutable_encrypted_data()->set_ciphertext(
      ConcatenatePayload((*cipher)->EncapsulatedKey(), *ciphertext));
  return response;
}

ExecutionResultOr<HpkeDecryptResponse>
CryptoClientProvider::HpkeDecryptUsingExternalInterface(
    const HpkeDecryptRequest& decrypt_request) noexcept {
  auto keyset_handle_or = CreateKeysetHandle(decrypt_request.tink_key_binary());
  if (!keyset_handle_or.Successful()) {
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, keyset_handle_or.result(),
              "Creating KeysetHandle failed with error.");
    return keyset_handle_or.result();
  }

  auto primitive_or = (*keyset_handle_or)->GetPrimitive<HybridDecrypt>();
  if (!primitive_or.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_TINK_PRIMITIVE);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Creating Hpke Decrypt Primitive failed with error %s.",
              primitive_or.status().ToString().c_str());
    return execution_result;
  }

  auto decrypt_result_or =
      (*primitive_or)
          ->Decrypt(decrypt_request.encrypted_data().ciphertext(),
                    decrypt_request.shared_info());
  if (!decrypt_result_or.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_HPKE_DECRYPT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Hpke decryption failed with error %s.",
              decrypt_result_or.status().ToString().c_str());
    return execution_result;
  }

  HpkeDecryptResponse response;
  response.set_payload(std::move(*decrypt_result_or));
  return response;
}

ExecutionResultOr<HpkeDecryptResponse> CryptoClientProvider::HpkeDecryptSync(
    const HpkeDecryptRequest& decrypt_request) noexcept {
  if (!decrypt_request.has_raw_key_with_params() &&
      !decrypt_request.has_tink_key_binary()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_MISSING_KEY);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "HPKE decryption failed.");
    return execution_result;
  }

  // Use Tink's external interface for non bidirectional decryption when input
  // the tink_binary_key.
  if (!decrypt_request.is_bidirectional() &&
      decrypt_request.has_tink_key_binary()) {
    return HpkeDecryptUsingExternalInterface(decrypt_request);
  }

  string raw_key;
  tink::HpkeParams tink_hpke_params;
  auto& encoded_key = decrypt_request.has_raw_key_with_params()
                          ? decrypt_request.raw_key_with_params().raw_key()
                          : decrypt_request.tink_key_binary();

  auto decoded_key_or = Base64Decode(encoded_key);
  if (!decoded_key_or.Successful()) {
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, decoded_key_or.result(),
              "HPKE decryption failed.");
    return decoded_key_or.result();
  }
  if (decrypt_request.has_raw_key_with_params()) {
    raw_key = decoded_key_or.release();
    auto tink_hpke_params_or = ToTinkHpkeParamsFromScp(
        decrypt_request.raw_key_with_params().hpke_params());
    RETURN_AND_LOG_IF_FAILURE(tink_hpke_params_or.result(),
                              kCryptoClientProvider, kZeroUuid,
                              "Invalid HpkeParams");
    tink_hpke_params = std::move(*tink_hpke_params_or);
  } else {
    auto key_set_or = CreateKeyset(decoded_key_or.release());
    RETURN_IF_FAILURE(key_set_or.result());
    HpkePrivateKey private_key;
    if (!private_key.ParseFromString(
            key_set_or.value()->key(0).key_data().value())) {
      auto execution_result = FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PRIVATE_KEY_FAILED);
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                "Hpke decryption failed with error.");
      return execution_result;
    }
    raw_key = private_key.private_key();
    auto tink_hpke_params_or =
        ToTinkHpkeParamsFromTinkProto(private_key.public_key().params());
    RETURN_AND_LOG_IF_FAILURE(tink_hpke_params_or.result(),
                              kCryptoClientProvider, kZeroUuid,
                              "Invalid HpkeParams");
    tink_hpke_params = std::move(*tink_hpke_params_or);
  }

  auto splitted_ciphertext = SplitPayload(
      tink_hpke_params.kem, decrypt_request.encrypted_data().ciphertext());
  if (!splitted_ciphertext.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_SPLIT_CIPHERTEXT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Hpke decryption failed with error %s.",
              splitted_ciphertext.status().ToString().c_str());
    return execution_result;
  }

  auto cipher = HpkeContext::SetupRecipient(
      tink_hpke_params, SecretDataFromStringView(raw_key),
      splitted_ciphertext->encapsulated_key, decrypt_request.shared_info());

  if (!cipher.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Hpke decryption failed with error %s.",
              cipher.status().ToString().c_str());
    return execution_result;
  }

  auto payload = (*cipher)->Open(splitted_ciphertext->ciphertext,
                                 "" /*Empty associated data*/);
  if (!payload.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_HPKE_DECRYPT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Hpke decryption failed with error %s.",
              payload.status().ToString().c_str());
    return execution_result;
  }

  HpkeDecryptResponse response;
  if (decrypt_request.is_bidirectional()) {
    auto secret =
        (*cipher)->Export(decrypt_request.exporter_context().empty()
                              ? kDefaultExporterContext
                              : decrypt_request.exporter_context(),
                          GetSecretLength(decrypt_request.secret_length()));
    if (!secret.ok()) {
      auto execution_result = FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_SECRET_EXPORT_FAILED);
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                "Hpke decryption failed with error %s.",
                secret.status().ToString().c_str());
      return execution_result;
    }
    response.set_secret(string(SecretDataAsStringView(*secret)));
  }

  response.set_payload(*payload);

  return response;
}

ExecutionResultOr<AeadEncryptResponse> CryptoClientProvider::AeadEncryptSync(
    const AeadEncryptRequest& request) noexcept {
  SecretData key = SecretDataFromStringView(request.secret());
  auto cipher = AesGcmBoringSsl::New(key);
  if (!cipher.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Aead encryption failed with error %s.",
              cipher.status().ToString().c_str());
    return execution_result;
  }
  auto ciphertext =
      (*cipher)->Encrypt(request.payload(), request.shared_info());
  if (!ciphertext.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_AEAD_ENCRYPT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Aead encryption failed with error %s.",
              ciphertext.status().ToString().c_str());
    return execution_result;
  }
  AeadEncryptResponse response;
  response.mutable_encrypted_data()->set_ciphertext((*ciphertext));
  return response;
}

ExecutionResultOr<AeadDecryptResponse> CryptoClientProvider::AeadDecryptSync(
    const AeadDecryptRequest& request) noexcept {
  SecretData key = SecretDataFromStringView(request.secret());
  auto cipher = AesGcmBoringSsl::New(key);
  if (!cipher.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Aead decryption failed with error %s.",
              cipher.status().ToString().c_str());
    return execution_result;
  }
  auto payload = (*cipher)->Decrypt(request.encrypted_data().ciphertext(),
                                    request.shared_info());
  if (!payload.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_AEAD_DECRYPT_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Aead decryption failed with error %s.",
              payload.status().ToString().c_str());
    return execution_result;
  }
  AeadDecryptResponse response;
  response.set_payload((*payload));
  return response;
}

ExecutionResultOr<unique_ptr<StreamingAead>> CreateSaead(
    cmrt::sdk::crypto_service::v1::StreamingAeadParams saead_params) {
  if (saead_params.has_aes_ctr_hmac_key()) {
    AesCtrHmacStreamingKey key;
    auto proto_key = saead_params.aes_ctr_hmac_key();
    auto decoded_key_or =
        proto_key.has_tink_key_binary()
            ? Base64Decode(proto_key.tink_key_binary())
            : Base64Decode(proto_key.raw_key_with_params().key_value());
    if (!decoded_key_or.Successful()) {
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, decoded_key_or.result(),
                "Decoding AesCtrHmacStreamingKey key failed with "
                "error.");
      return decoded_key_or.result();
    }
    auto decoded_key = decoded_key_or.release();
    if (proto_key.has_raw_key_with_params()) {
      auto raw_key = proto_key.raw_key_with_params();
      key.set_version(raw_key.version());
      key.set_key_value(decoded_key);
      key.mutable_params()->set_ciphertext_segment_size(
          raw_key.params().ciphertext_segment_size());
      key.mutable_params()->set_derived_key_size(
          raw_key.params().derived_key_size());
      key.mutable_params()->set_hkdf_hash_type(
          kHashTypeMap.at(raw_key.params().hkdf_hash_type()));
      key.mutable_params()->mutable_hmac_params()->set_hash(
          kHashTypeMap.at(raw_key.params().hmac_params().hash()));
      key.mutable_params()->mutable_hmac_params()->set_tag_size(
          raw_key.params().hmac_params().tag_size());
    } else if (proto_key.has_tink_key_binary()) {
      auto keyset_or = CreateKeyset(decoded_key);
      RETURN_IF_FAILURE(keyset_or.result());
      if (!key.ParseFromString(keyset_or.value()->key(0).key_data().value())) {
        auto execution_result = FailureExecutionResult(
            SC_CRYPTO_CLIENT_PROVIDER_PARSE_STREAMING_AEAD_KEY_FAILED);
        SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                  "Failed to construct AesCtrHmacStreamingKey.");
        return execution_result;
      }
    } else {
      auto execution_result =
          FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                "No config found for AesCtrHmacStreamingKey.");
      return execution_result;
    }

    auto streaming_result =
        AesCtrHmacStreamingKeyManager().GetPrimitive<StreamingAead>(key);
    if (!streaming_result.ok()) {
      auto execution_result =
          FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                "AesCtrHmac streaming aead creation failed with error %s.",
                streaming_result.status().ToString().c_str());
      return execution_result;
    }
    return move(streaming_result.value());
  } else if (saead_params.has_aes_gcm_hkdf_key()) {
    AesGcmHkdfStreamingKey key;
    auto proto_key = saead_params.aes_gcm_hkdf_key();
    auto decoded_key_or =
        proto_key.has_tink_key_binary()
            ? Base64Decode(proto_key.tink_key_binary())
            : Base64Decode(proto_key.raw_key_with_params().key_value());
    if (!decoded_key_or.Successful()) {
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, decoded_key_or.result(),
                "Decoding AesGcmHkdfStreamingKey key failed with error.");
      return decoded_key_or.result();
    }
    auto decoded_key = decoded_key_or.release();
    if (proto_key.has_raw_key_with_params()) {
      auto raw_key = proto_key.raw_key_with_params();
      key.set_version(raw_key.version());
      key.set_key_value(decoded_key);
      key.mutable_params()->set_ciphertext_segment_size(
          raw_key.params().ciphertext_segment_size());
      key.mutable_params()->set_derived_key_size(
          raw_key.params().derived_key_size());
      key.mutable_params()->set_hkdf_hash_type(
          kHashTypeMap.at(raw_key.params().hkdf_hash_type()));
    } else if (proto_key.has_tink_key_binary()) {
      auto keyset_or = CreateKeyset(decoded_key);
      RETURN_IF_FAILURE(keyset_or.result());
      if (!key.ParseFromString(keyset_or.value()->key(0).key_data().value())) {
        auto execution_result = FailureExecutionResult(
            SC_CRYPTO_CLIENT_PROVIDER_PARSE_STREAMING_AEAD_KEY_FAILED);
        SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                  "Failed to construct AesGcmHkdfStreamingKey.");
        return execution_result;
      }
    } else {
      auto execution_result =
          FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                "No config found for AesGcmHkdfStreamingKey.");
      return execution_result;
    }

    auto streaming_result =
        AesGcmHkdfStreamingKeyManager().GetPrimitive<StreamingAead>(key);
    if (!streaming_result.ok()) {
      auto execution_result =
          FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
      SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
                "AesGcmHkdf streaming aead creation failed with error %s.",
                streaming_result.status().ToString().c_str());
      return execution_result;
    }
    return move(streaming_result.value());
  } else {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_SAEAD_CONFIG_MISSING);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "No streaming aead config provided");
    return execution_result;
  }
}

ExecutionResultOr<unique_ptr<OutputStream>>
CryptoClientProvider::AeadEncryptStreamSync(
    const google::scp::cpio::AeadEncryptStreamRequest& request) noexcept {
  auto result = CreateSaead(request.saead_params);
  RETURN_IF_FAILURE(result.result());
  unique_ptr<StreamingAead> saead = move(result.value());

  std::unique_ptr<OutputStream> ct_destination(
      absl::make_unique<OstreamOutputStream>(
          make_unique<ostream>(move(request.ciphertext_stream->rdbuf()))));
  // Encrypt the plaintext.
  auto enc_stream_result = saead->NewEncryptingStream(
      move(ct_destination), request.saead_params.shared_info());
  if (!enc_stream_result.ok()) {
    auto execution_result = FailureExecutionResult(
        core::errors::
            SC_CRYPTO_CLIENT_PROVIDER_SAEAD_CREATE_ENCRYPT_STREAM_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Create saead encryption stream failed with error %s.",
              enc_stream_result.status().ToString().c_str());
    return execution_result;
  }
  return move(enc_stream_result.value());
}

ExecutionResultOr<unique_ptr<InputStream>>
CryptoClientProvider::AeadDecryptStreamSync(
    const google::scp::cpio::AeadDecryptStreamRequest& request) noexcept {
  auto result = CreateSaead(request.saead_params);
  RETURN_IF_FAILURE(result.result());
  unique_ptr<StreamingAead> saead = move(result.value());

  // Prepare ciphertext source stream.
  unique_ptr<InputStream> ct_source(make_unique<IstreamInputStream>(
      make_unique<istream>(move(request.ciphertext_stream->rdbuf()))));
  // Decrypt the ciphertext.
  auto dec_stream_result = saead->NewDecryptingStream(
      move(ct_source), request.saead_params.shared_info());
  if (!dec_stream_result.ok()) {
    auto execution_result = FailureExecutionResult(
        core::errors::
            SC_CRYPTO_CLIENT_PROVIDER_SAEAD_CREATE_DECRYPT_STREAM_FAILED);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Create saead decryption stream failed with error %s.",
              dec_stream_result.status().ToString().c_str());
    return execution_result;
  }
  return move(dec_stream_result.value());
}

ExecutionResultOr<ComputeMacResponse> CryptoClientProvider::ComputeMacSync(
    const ComputeMacRequest& request) noexcept {
  if (request.key().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_MISSING_KEY);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Key is missing in the ComputeMacRequest.");
    return execution_result;
  }

  if (request.data().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_MISSING_DATA);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Data is missing in the ComputeMacRequest.");
    return execution_result;
  }

  auto keyset_handle_or = CreateKeysetHandle(request.key());
  if (!keyset_handle_or.Successful()) {
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, keyset_handle_or.result(),
              "Creating KeysetHandle failed with error.");
    return keyset_handle_or.result();
  }

  auto mac_primitive_or = (*keyset_handle_or)->GetPrimitive<Mac>();
  if (!mac_primitive_or.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_TINK_PRIMITIVE);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Creating mac failed with error %s.",
              mac_primitive_or.status().ToString().c_str());
    return execution_result;
  }

  auto compute_result_or = (*mac_primitive_or)->ComputeMac(request.data());
  if (!compute_result_or.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CANNOT_COMPUTE_MAC);
    SCP_ERROR(kCryptoClientProvider, kZeroUuid, execution_result,
              "Computing mac failed with error %s.",
              compute_result_or.status().ToString().c_str());
    return execution_result;
  }

  ComputeMacResponse response;
  response.set_mac(std::move(*compute_result_or));
  return response;
}
}  // namespace google::scp::cpio::client_providers
