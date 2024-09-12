// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpio/client_providers/crypto_client_provider/src/crypto_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <tink/binary_keyset_writer.h>
#include <tink/cleartext_keyset_handle.h>
#include <tink/hybrid/hybrid_key_templates.h>
#include <tink/hybrid/internal/hpke_context.h>
#include <tink/input_stream.h>
#include <tink/mac/mac_key_templates.h>
#include <tink/output_stream.h>
#include <tink/streaming_aead.h>
#include <tink/subtle/aes_ctr_hmac_streaming.h>
#include <tink/subtle/aes_gcm_hkdf_streaming.h>
#include <tink/subtle/common_enums.h>
#include <tink/subtle/random.h>
#include <tink/util/ostream_output_stream.h>
#include <tink/util/secret_data.h>

#include "absl/strings/escaping.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "core/utils/src/base64.h"
#include "core/utils/src/error_codes.h"
#include "cpio/client_providers/crypto_client_provider/src/error_codes.h"
#include "proto/aes_ctr_hmac_streaming.pb.h"
#include "proto/aes_gcm_hkdf_streaming.pb.h"
#include "proto/hpke.pb.h"
#include "proto/tink.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

using absl::Base64Escape;
using absl::HexStringToBytes;
using crypto::tink::BinaryKeysetWriter;
using crypto::tink::CleartextKeysetHandle;
using crypto::tink::HybridKeyTemplates;
using crypto::tink::InputStream;
using crypto::tink::KeysetHandle;
using crypto::tink::MacKeyTemplates;
using crypto::tink::OutputStream;
using crypto::tink::subtle::AesCtrHmacStreaming;
using crypto::tink::subtle::AesGcmHkdfStreaming;
using crypto::tink::subtle::Random;
using crypto::tink::util::OkStatus;
using crypto::tink::util::OstreamOutputStream;
using crypto::tink::util::SecretData;
using crypto::tink::util::Status;
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
using google::cmrt::sdk::crypto_service::v1::StreamingAeadParams;
using google::crypto::tink::AesCtrHmacStreamingKey;
using google::crypto::tink::AesGcmHkdfStreamingKey;
using google::crypto::tink::HpkePrivateKey;
using google::crypto::tink::HpkePublicKey;
using google::crypto::tink::Keyset;
using google::crypto::tink::KeyStatusType;
using google::crypto::tink::OutputPrefixType;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_MISSING_DATA;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_MISSING_KEY;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PUBLIC_KEY_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_READ_KEYSET_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::AeadDecryptStreamRequest;
using std::atomic;
using std::function;
using std::istream;
using std::make_shared;
using std::make_unique;
using std::min;
using std::move;
using std::ostream;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::stringstream;
using std::unique_ptr;
using std::vector;

namespace google::scp::cpio::client_providers::test {
constexpr char kKeyId[] = "key_id";
constexpr char kSharedInfo[] = "shared_info";
constexpr char kPayload[] = "payload";
constexpr char kSecret128[] = "000102030405060708090a0b0c0d0e0f";
constexpr char kSecret256[] =
    "000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f";
constexpr char kPublicKeyForChacha20[] =
    "4310ee97d88cc1f088a5576c77ab0cf5c3ac797f3d95139c6c84b5429c59662a";
constexpr char kPublicKeyForAes128Gcm[] =
    "3948cfe0ad1ddb695d780e59077195da6c56506b027329794ab02bca80815c4d";
constexpr char kPublicKeyForP256[] =
    "04fe8c19ce0905191ebc298a9245792531f26f0cece2460639e8bc39cb7f706a826a779b4c"
    "f969b8a0e539c7f62fb3d30ad6aa8f80e30f1d128aafd68a2ce72ea0";
constexpr char kDecryptedPrivateKeyForChacha20[] =
    "8057991eef8f1f1af18f4a9491d16a1ce333f695d4db8e38da75975c4478e0fb";
constexpr char kDecryptedPrivateKeyForAes128Gcm[] =
    "4612c550263fc8ad58375df3f557aac531d26850903e55a9f23f21d8534e8ac8";
constexpr char kDecryptedPrivateKeyForP256[] =
    "f3ce7fdae57e1a310d87f1ebbde6f328be0a99cdbcadf4d6589cf29de4b8ffd2";

class CryptoClientProviderTest : public ScpTestBase {
 protected:
  void SetUp() override {
    auto options = make_shared<CryptoClientOptions>();
    client_ = make_unique<CryptoClientProvider>(options);

    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());

    auto keyset_handle_1_or = KeysetHandle::GenerateNew(
        HybridKeyTemplates::HpkeX25519HkdfSha256ChaCha20Poly1305Raw());
    HpkeX25519HkdfSha256ChaCha20Poly1305Raw_encoded_tink_private_key =
        EncodeKeyset(keyset_handle_1_or->get());
    HpkeX25519HkdfSha256ChaCha20Poly1305Raw_encoded_tink_public_key =
        WrapHpkePublicKey(keyset_handle_1_or->get());

    auto keyset_handle_2_or = KeysetHandle::GenerateNew(
        HybridKeyTemplates::HpkeX25519HkdfSha256Aes256GcmRaw());
    HpkeX25519HkdfSha256Aes256GcmRaw_encoded_tink_private_key =
        EncodeKeyset(keyset_handle_2_or->get());
    HpkeX25519HkdfSha256Aes256GcmRaw_encoded_tink_public_key =
        WrapHpkePublicKey(keyset_handle_2_or->get());

    auto keyset_handle_3_or = KeysetHandle::GenerateNew(
        HybridKeyTemplates::HpkeX25519HkdfSha256Aes128GcmRaw());
    HpkeX25519HkdfSha256Aes128GcmRaw_encoded_tink_private_key =
        EncodeKeyset(keyset_handle_3_or->get());
    HpkeX25519HkdfSha256Aes128GcmRaw_encoded_tink_public_key =
        WrapHpkePublicKey(keyset_handle_3_or->get());

    auto keyset_handle_4_or = KeysetHandle::GenerateNew(
        HybridKeyTemplates::
            EciesP256HkdfHmacSha256Aes128GcmCompressedWithoutPrefix());
    EciesP256HkdfHmacSha256Aes128GcmRaw_encoded_tink_private_key =
        EncodeKeyset(keyset_handle_4_or->get());
    EciesP256HkdfHmacSha256Aes128GcmRaw_encoded_tink_public_key =
        WrapHpkePublicKey(keyset_handle_4_or->get(), true);
  }

  string WrapHpkePublicKey(KeysetHandle* keyset_handle,
                           bool is_ecies_key = false) {
    auto keyset = CleartextKeysetHandle::GetKeyset(*keyset_handle);
    HpkePrivateKey private_key;
    private_key.ParseFromString(keyset.key(0).key_data().value());
    HpkePublicKey public_key = private_key.public_key();

    Keyset key;
    key.set_primary_key_id(123);
    key.add_key();
    key.mutable_key(0)->set_key_id(123);
    if (is_ecies_key) {
      key.mutable_key(0)->mutable_key_data()->set_type_url(
          "type.googleapis.com/google.crypto.tink.EciesAeadHkdfPublicKey");
    } else {
      key.mutable_key(0)->mutable_key_data()->set_type_url(
          "type.googleapis.com/google.crypto.tink.HpkePublicKey");
    }
    key.mutable_key(0)->mutable_key_data()->set_key_material_type(
        google::crypto::tink::KeyData::ASYMMETRIC_PUBLIC);
    key.mutable_key(0)->mutable_key_data()->set_value(
        public_key.SerializeAsString());
    key.mutable_key(0)->set_status(KeyStatusType::ENABLED);
    key.mutable_key(0)->set_output_prefix_type(OutputPrefixType::RAW);

    return *Base64Encode(key.SerializeAsString());
  }

  string EncodeKeyset(KeysetHandle* keyset_handle) {
    std::stringbuf key_buf(std::ios_base::out);
    auto keyset_writer =
        BinaryKeysetWriter::New(std::make_unique<std::ostream>(&key_buf));
    auto write_result =
        CleartextKeysetHandle::Write(keyset_writer->get(), *keyset_handle);
    return *Base64Encode(key_buf.str());
  }

  void TearDown() override { EXPECT_SUCCESS(client_->Stop()); }

  static HpkeParams GetDefaultHpkeParams() {
    HpkeParams hpke_params;
    hpke_params.set_kem(HpkeKem::DHKEM_X25519_HKDF_SHA256);
    hpke_params.set_kdf(HpkeKdf::HKDF_SHA256);
    hpke_params.set_aead(HpkeAead::CHACHA20_POLY1305);
    return hpke_params;
  }

  string GetEncodedPublicKey(crypto::tink::HpkeKem kem,
                             crypto::tink::HpkeKdf kdf,
                             crypto::tink::HpkeAead aead) {
    string encoded_key;
    switch (aead) {
      case crypto::tink::HpkeAead::CHACHA20_POLY1305:
        encoded_key =
            HpkeX25519HkdfSha256ChaCha20Poly1305Raw_encoded_tink_public_key;
        break;
      case crypto::tink::HpkeAead::AES_256_GCM:
        encoded_key = HpkeX25519HkdfSha256Aes256GcmRaw_encoded_tink_public_key;
        break;
      case crypto::tink::HpkeAead::AES_128_GCM:
        encoded_key = HpkeX25519HkdfSha256Aes128GcmRaw_encoded_tink_public_key;
        break;
    }
    return encoded_key;
  }

  crypto::tink::HpkeKem ToTinkHpkeKem(HpkeKem kem) {
    switch (kem) {
      case HpkeKem::DHKEM_X25519_HKDF_SHA256:
        return crypto::tink::HpkeKem::DHKEM_X25519_HKDF_SHA256;
      default:
        return crypto::tink::HpkeKem::KEM_UNKNOWN;
    }
  }

  crypto::tink::HpkeKdf ToTinkHpkeKdf(HpkeKdf kdf) {
    switch (kdf) {
      case HpkeKdf::HKDF_SHA256:
        return crypto::tink::HpkeKdf::HKDF_SHA256;
      default:
        return crypto::tink::HpkeKdf::KDF_UNKNOWN;
    }
  }

  crypto::tink::HpkeAead ToTinkHpkeAead(HpkeAead aead) {
    switch (aead) {
      case HpkeAead::CHACHA20_POLY1305:
        return crypto::tink::HpkeAead::CHACHA20_POLY1305;
      case HpkeAead::AES_256_GCM:
        return crypto::tink::HpkeAead::AES_256_GCM;
      case HpkeAead::AES_128_GCM:
        return crypto::tink::HpkeAead::AES_128_GCM;
      default:
        return crypto::tink::HpkeAead::AEAD_UNKNOWN;
    }
  }

  string GetEncodedPrivateKey(crypto::tink::HpkeKem kem,
                              crypto::tink::HpkeKdf kdf,
                              crypto::tink::HpkeAead aead) {
    string encoded_key;
    switch (aead) {
      case crypto::tink::HpkeAead::CHACHA20_POLY1305:
        encoded_key =
            HpkeX25519HkdfSha256ChaCha20Poly1305Raw_encoded_tink_private_key;
        break;
      case crypto::tink::HpkeAead::AES_256_GCM:
        encoded_key = HpkeX25519HkdfSha256Aes256GcmRaw_encoded_tink_private_key;
        break;
      case crypto::tink::HpkeAead::AES_128_GCM:
        encoded_key = HpkeX25519HkdfSha256Aes128GcmRaw_encoded_tink_private_key;
        break;
    }
    return encoded_key;
  }

  HpkeEncryptRequest CreateHpkeEncryptRequestWithTinkKey(
      crypto::tink::HpkeKem kem, crypto::tink::HpkeKdf kdf,
      crypto::tink::HpkeAead aead, bool is_bidirectional) {
    string encoded_key = GetEncodedPublicKey(kem, kdf, aead);
    HpkeEncryptRequest request;
    request.set_tink_key_binary(encoded_key);
    request.set_key_id(kKeyId);
    request.set_is_bidirectional(is_bidirectional);
    request.set_shared_info(string(kSharedInfo));
    request.set_payload(string(kPayload));
    return request;
  }

  Keyset CreateHpkePublicKeySet(crypto::tink::HpkeKem kem,
                                crypto::tink::HpkeKdf kdf,
                                crypto::tink::HpkeAead aead) {
    HpkePublicKey hpke_public_key;
    hpke_public_key.mutable_params()->set_kem(kem);
    hpke_public_key.mutable_params()->set_kdf(kdf);
    hpke_public_key.mutable_params()->set_aead(aead);
    if (kem == crypto::tink::HpkeKem::DHKEM_P256_HKDF_SHA256) {
      hpke_public_key.set_public_key(HexStringToBytes(kPublicKeyForP256));
    } else {
      hpke_public_key.set_public_key(HexStringToBytes(kPublicKeyForChacha20));
    }
    Keyset key;
    key.set_primary_key_id(123);
    key.add_key();
    key.mutable_key(0)->set_key_id(123);
    key.mutable_key(0)->mutable_key_data()->set_type_url(
        "type.googleapis.com/google.crypto.tink.HpkePublicKey");
    key.mutable_key(0)->mutable_key_data()->set_key_material_type(
        google::crypto::tink::KeyData::ASYMMETRIC_PUBLIC);
    key.mutable_key(0)->mutable_key_data()->set_value(
        hpke_public_key.SerializeAsString());
    key.mutable_key(0)->set_status(KeyStatusType::ENABLED);
    key.mutable_key(0)->set_output_prefix_type(OutputPrefixType::RAW);
    return key;
  }

  HpkeEncryptRequest CreateHpkeEncryptRequest(crypto::tink::HpkeKem kem,
                                              crypto::tink::HpkeKdf kdf,
                                              crypto::tink::HpkeAead aead,
                                              bool is_bidirectional = true) {
    auto key = CreateHpkePublicKeySet(kem, kdf, aead);
    auto encoded_key = *Base64Encode(key.SerializeAsString());
    HpkeEncryptRequest request;
    request.set_tink_key_binary(encoded_key);
    request.set_key_id(kKeyId);
    request.set_shared_info(string(kSharedInfo));
    request.set_is_bidirectional(is_bidirectional);
    request.set_payload(string(kPayload));
    return request;
  }

  Keyset CreateHpkePrivateKeySet(crypto::tink::HpkeKem kem,
                                 crypto::tink::HpkeKdf kdf,
                                 crypto::tink::HpkeAead aead) {
    HpkePrivateKey hpke_private_key;
    hpke_private_key.mutable_public_key()->mutable_params()->set_kem(kem);
    hpke_private_key.mutable_public_key()->mutable_params()->set_kdf(kdf);
    hpke_private_key.mutable_public_key()->mutable_params()->set_aead(aead);

    if (kem == crypto::tink::HpkeKem::DHKEM_P256_HKDF_SHA256) {
      hpke_private_key.mutable_public_key()->set_public_key(
          HexStringToBytes(kPublicKeyForP256));
      hpke_private_key.set_private_key(
          HexStringToBytes(kDecryptedPrivateKeyForP256));
    } else {
      if (aead == crypto::tink::HpkeAead::AES_128_GCM) {
        hpke_private_key.mutable_public_key()->set_public_key(
            HexStringToBytes(kPublicKeyForAes128Gcm));
        hpke_private_key.set_private_key(
            HexStringToBytes(kDecryptedPrivateKeyForAes128Gcm));
      } else {
        hpke_private_key.mutable_public_key()->set_public_key(
            HexStringToBytes(kPublicKeyForChacha20));
        hpke_private_key.set_private_key(
            HexStringToBytes(kDecryptedPrivateKeyForChacha20));
      }
    }

    Keyset key;
    key.set_primary_key_id(123);
    key.add_key();
    key.mutable_key(0)->mutable_key_data()->set_value(
        hpke_private_key.SerializeAsString());
    key.mutable_key(0)->set_key_id(123);
    key.mutable_key(0)->mutable_key_data()->set_type_url(
        "type.googleapis.com/google.crypto.tink.HpkePrivateKey");
    key.mutable_key(0)->mutable_key_data()->set_key_material_type(
        google::crypto::tink::KeyData::ASYMMETRIC_PRIVATE);
    key.mutable_key(0)->set_status(KeyStatusType::ENABLED);
    key.mutable_key(0)->set_output_prefix_type(OutputPrefixType::RAW);

    return key;
  }

  HpkeDecryptRequest CreateHpkeDecryptRequest(crypto::tink::HpkeKem kem,
                                              crypto::tink::HpkeKdf kdf,
                                              crypto::tink::HpkeAead aead,
                                              bool is_bidirectional = true) {
    auto key = CreateHpkePrivateKeySet(kem, kdf, aead);
    auto encoded_key = *Base64Encode(key.SerializeAsString());
    HpkeDecryptRequest request;
    request.set_tink_key_binary(encoded_key);
    request.set_shared_info(string(kSharedInfo));
    request.mutable_encrypted_data()->set_ciphertext("abcdefgh");
    request.mutable_encrypted_data()->set_key_id(string(kKeyId));
    request.set_is_bidirectional(is_bidirectional);
    return request;
  }

  HpkeEncryptRequest CreateHpkeEncryptRequest(
      bool is_bidirectional, bool is_raw_key,
      const string& encoded_public_key = "",
      const string& exporter_context = "",
      HpkeParams hpke_params_from_request = GetDefaultHpkeParams()) {
    HpkeEncryptRequest request;
    if (is_raw_key) {
      *request.mutable_raw_key_with_params()->mutable_hpke_params() =
          hpke_params_from_request;
      if (encoded_public_key.empty()) {
        if (hpke_params_from_request.aead() == HpkeAead::AES_128_GCM) {
          request.mutable_raw_key_with_params()->set_raw_key(
              Base64Escape(HexStringToBytes(kPublicKeyForAes128Gcm)));
        } else {
          request.mutable_raw_key_with_params()->set_raw_key(
              Base64Escape(HexStringToBytes(kPublicKeyForChacha20)));
        }
      } else {
        request.mutable_raw_key_with_params()->set_raw_key(encoded_public_key);
      }
    } else {
      crypto::tink::HpkeAead aead =
          hpke_params_from_request.aead() == HpkeAead::AES_128_GCM
              ? crypto::tink::HpkeAead::AES_128_GCM
              : crypto::tink::HpkeAead::CHACHA20_POLY1305;
      if (encoded_public_key.empty()) {
        string encoded_key = GetEncodedPublicKey(
            ToTinkHpkeKem(hpke_params_from_request.kem()),
            ToTinkHpkeKdf(hpke_params_from_request.kdf()),
            ToTinkHpkeAead(hpke_params_from_request.aead()));
        request.set_tink_key_binary(encoded_key);
      } else {
        request.set_tink_key_binary(encoded_public_key);
      }
    }
    request.set_key_id(kKeyId);
    request.set_shared_info(string(kSharedInfo));
    request.set_payload(string(kPayload));
    request.set_is_bidirectional(is_bidirectional);
    request.set_exporter_context(exporter_context);
    return request;
  }

  HpkeEncryptRequest CreateHpkeEncryptRequestWithEciesKey() {
    HpkeEncryptRequest request;
    request.set_tink_key_binary(
        EciesP256HkdfHmacSha256Aes128GcmRaw_encoded_tink_public_key);
    request.set_key_id(kKeyId);
    request.set_shared_info(string(kSharedInfo));
    request.set_payload(string(kPayload));
    return request;
  }

  void AssertHpkeEncryptResponse(
      bool is_bidirectional,
      const ExecutionResultOr<HpkeEncryptResponse>& response_or,
      const ExecutionResult& expected_result = SuccessExecutionResult()) {
    if (expected_result.Successful()) {
      EXPECT_SUCCESS(response_or.result());
      auto response = move(*response_or);
      if (is_bidirectional) {
        EXPECT_FALSE(response.secret().empty());
      } else {
        EXPECT_TRUE(response.secret().empty());
      }
      EXPECT_EQ(response.encrypted_data().key_id(), kKeyId);
    } else {
      EXPECT_THAT(response_or.result(), ResultIs(expected_result));
    }
  }

  HpkeDecryptRequest CreateHpkeDecryptRequest(
      string_view ciphertext, bool is_bidirectional, bool is_raw_key,
      const string& secret,
      const ExecutionResult& decrypt_private_key_result =
          SuccessExecutionResult(),
      const string& exporter_context = "",
      HpkeParams hpke_params_from_request = GetDefaultHpkeParams()) {
    HpkeDecryptRequest request;
    if (is_raw_key) {
      request.mutable_raw_key_with_params()->mutable_hpke_params()->set_kem(
          HpkeKem::DHKEM_X25519_HKDF_SHA256);
      request.mutable_raw_key_with_params()->mutable_hpke_params()->set_kdf(
          HpkeKdf::HKDF_SHA256);
      if (hpke_params_from_request.aead() == HpkeAead::AES_128_GCM) {
        auto encoded_key =
            *Base64Encode(HexStringToBytes(kDecryptedPrivateKeyForAes128Gcm));
        request.mutable_raw_key_with_params()->set_raw_key(encoded_key);
        request.mutable_raw_key_with_params()->mutable_hpke_params()->set_aead(
            HpkeAead::AES_128_GCM);
      } else {
        auto encoded_key =
            *Base64Encode(HexStringToBytes(kDecryptedPrivateKeyForChacha20));
        request.mutable_raw_key_with_params()->set_raw_key(encoded_key);
        request.mutable_raw_key_with_params()->mutable_hpke_params()->set_aead(
            HpkeAead::CHACHA20_POLY1305);
      }
    } else {
      crypto::tink::HpkeAead aead =
          hpke_params_from_request.aead() == HpkeAead::AES_128_GCM
              ? crypto::tink::HpkeAead::AES_128_GCM
              : crypto::tink::HpkeAead::CHACHA20_POLY1305;

      string encoded_private_key;
      if (decrypt_private_key_result.Successful()) {
        encoded_private_key = GetEncodedPrivateKey(
            ToTinkHpkeKem(hpke_params_from_request.kem()),
            ToTinkHpkeKdf(hpke_params_from_request.kdf()),
            ToTinkHpkeAead(hpke_params_from_request.aead()));
        request.set_tink_key_binary(encoded_private_key);
      } else if (decrypt_private_key_result.status_code ==
                 SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH) {
        request.set_tink_key_binary("invalid");
      } else {
        encoded_private_key = *Base64Encode("invalid");
        request.set_tink_key_binary(encoded_private_key);
      }
    }
    request.set_shared_info(string(kSharedInfo));
    request.set_is_bidirectional(is_bidirectional);
    request.mutable_encrypted_data()->set_ciphertext(string(ciphertext));
    request.mutable_encrypted_data()->set_key_id(string(kKeyId));
    request.set_exporter_context(exporter_context);
    return request;
  }

  HpkeDecryptRequest CreateHpkeDecryptRequestWithEciesKey(
      string_view ciphertext) {
    HpkeDecryptRequest request;
    request.set_tink_key_binary(
        EciesP256HkdfHmacSha256Aes128GcmRaw_encoded_tink_private_key);
    request.set_shared_info(string(kSharedInfo));
    request.mutable_encrypted_data()->set_ciphertext(string(ciphertext));
    request.mutable_encrypted_data()->set_key_id(string(kKeyId));
    return request;
  }

  void AssertHpkeDecryptResponse(
      const ExecutionResultOr<HpkeDecryptResponse>& response_or,
      const string& secret,
      const ExecutionResult& expected_result = SuccessExecutionResult()) {
    if (expected_result.Successful()) {
      EXPECT_SUCCESS(response_or.result());
      auto response = move(*response_or);
      EXPECT_EQ(response.payload(), kPayload);
      EXPECT_EQ(response.secret(), secret);
    } else {
      EXPECT_THAT(response_or.result(), ResultIs(expected_result));
    }
  }

  AeadEncryptRequest CreateAeadEncryptRequest(string_view secret) {
    AeadEncryptRequest request;
    request.set_shared_info(string(kSharedInfo));
    request.set_payload(string(kPayload));
    request.set_secret(HexStringToBytes(secret));
    return request;
  }

  AeadDecryptRequest CreateAeadDecryptRequest(string_view secret,
                                              string_view ciphertext) {
    AeadDecryptRequest request;
    request.set_shared_info(string(kSharedInfo));
    request.set_secret(HexStringToBytes(secret));
    request.mutable_encrypted_data()->set_ciphertext(string(ciphertext));
    return request;
  }

  unique_ptr<CryptoClientProvider> client_;
  string HpkeX25519HkdfSha256ChaCha20Poly1305Raw_encoded_tink_private_key;
  string HpkeX25519HkdfSha256ChaCha20Poly1305Raw_encoded_tink_public_key;
  string HpkeX25519HkdfSha256Aes256GcmRaw_encoded_tink_private_key;
  string HpkeX25519HkdfSha256Aes256GcmRaw_encoded_tink_public_key;
  string HpkeX25519HkdfSha256Aes128GcmRaw_encoded_tink_private_key;
  string HpkeX25519HkdfSha256Aes128GcmRaw_encoded_tink_public_key;
  string EciesP256HkdfHmacSha256Aes128GcmRaw_encoded_tink_private_key;
  string EciesP256HkdfHmacSha256Aes128GcmRaw_encoded_tink_public_key;
};

TEST_F(CryptoClientProviderTest, HpkeEncryptFailedWithInvalidHpkeKem) {
  auto encrypt_response_or = client_->HpkeEncryptSync(
      CreateHpkeEncryptRequest(crypto::tink::HpkeKem::DHKEM_P521_HKDF_SHA512,
                               crypto::tink::HpkeKdf::HKDF_SHA256,
                               crypto::tink::HpkeAead::AES_128_GCM, true));
  AssertHpkeEncryptResponse(
      false, encrypt_response_or,
      FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM));
}

TEST_F(CryptoClientProviderTest, HpkeEncryptDecryptSucceedWithHpkeKdfP256) {
  auto encrypt_response_or = client_->HpkeEncryptSync(
      CreateHpkeEncryptRequest(crypto::tink::HpkeKem::DHKEM_P256_HKDF_SHA256,
                               crypto::tink::HpkeKdf::HKDF_SHA256,
                               crypto::tink::HpkeAead::AES_128_GCM, false));
  AssertHpkeEncryptResponse(false, encrypt_response_or,
                            SuccessExecutionResult());
  auto decrypt_request =
      CreateHpkeDecryptRequest(crypto::tink::HpkeKem::DHKEM_P256_HKDF_SHA256,
                               crypto::tink::HpkeKdf::HKDF_SHA256,
                               crypto::tink::HpkeAead::AES_128_GCM, false);
  decrypt_request.mutable_encrypted_data()->set_ciphertext(
      encrypt_response_or->encrypted_data().ciphertext());
  auto decrypt_response_or = client_->HpkeDecryptSync(decrypt_request);
  AssertHpkeDecryptResponse(decrypt_response_or, encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest,
       BidirectionalHpkeEncryptDecryptSucceedWithHpkeKdfP256) {
  auto encrypt_response_or = client_->HpkeEncryptSync(
      CreateHpkeEncryptRequest(crypto::tink::HpkeKem::DHKEM_P256_HKDF_SHA256,
                               crypto::tink::HpkeKdf::HKDF_SHA256,
                               crypto::tink::HpkeAead::AES_128_GCM, true));
  AssertHpkeEncryptResponse(true, encrypt_response_or,
                            SuccessExecutionResult());
  auto decrypt_request =
      CreateHpkeDecryptRequest(crypto::tink::HpkeKem::DHKEM_P256_HKDF_SHA256,
                               crypto::tink::HpkeKdf::HKDF_SHA256,
                               crypto::tink::HpkeAead::AES_128_GCM, true);
  decrypt_request.mutable_encrypted_data()->set_ciphertext(
      encrypt_response_or->encrypted_data().ciphertext());
  auto decrypt_response_or = client_->HpkeDecryptSync(decrypt_request);
  AssertHpkeDecryptResponse(decrypt_response_or, encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest, HpkeEncryptFailedWithInvalidHpkeKdf) {
  auto encrypt_response_or = client_->HpkeEncryptSync(
      CreateHpkeEncryptRequest(crypto::tink::HpkeKem::DHKEM_X25519_HKDF_SHA256,
                               crypto::tink::HpkeKdf::HKDF_SHA384,
                               crypto::tink::HpkeAead::AES_128_GCM, true));
  AssertHpkeEncryptResponse(
      false, encrypt_response_or,
      FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM));
}

TEST_F(CryptoClientProviderTest, HpkeEncryptFailedWithoutKey) {
  auto encrypt_response_or = client_->HpkeEncryptSync(HpkeEncryptRequest());
  AssertHpkeEncryptResponse(
      false, encrypt_response_or,
      FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_MISSING_KEY));
}

TEST_F(CryptoClientProviderTest, HpkeDecryptFailedWithInvalidHpkeKem) {
  auto decrypt_response_or = client_->HpkeDecryptSync(
      CreateHpkeDecryptRequest(crypto::tink::HpkeKem::DHKEM_P521_HKDF_SHA512,
                               crypto::tink::HpkeKdf::HKDF_SHA256,
                               crypto::tink::HpkeAead::AES_128_GCM, true));
  AssertHpkeDecryptResponse(
      decrypt_response_or, "",
      FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM));
}

TEST_F(CryptoClientProviderTest, HpkeDecryptFailedWithInvalidHpkeKdf) {
  auto decrypt_response_or = client_->HpkeDecryptSync(
      CreateHpkeDecryptRequest(crypto::tink::HpkeKem::DHKEM_X25519_HKDF_SHA256,
                               crypto::tink::HpkeKdf::HKDF_SHA384,
                               crypto::tink::HpkeAead::AES_128_GCM, true));
  AssertHpkeDecryptResponse(
      decrypt_response_or, "",
      FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_UNSUPPORTED_ENCRYPTION_ALGORITHM));
}

TEST_F(CryptoClientProviderTest, HpkeDecryptFailedWithoutKey) {
  auto decrypt_response_or = client_->HpkeDecryptSync(HpkeDecryptRequest());
  AssertHpkeDecryptResponse(
      decrypt_response_or, "",
      FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_MISSING_KEY));
}

TEST_F(CryptoClientProviderTest, HpkeEncryptAndDecryptSuccessForOneDirection) {
  auto encrypt_request = CreateHpkeEncryptRequest(false /*is_bidirectional*/,
                                                  false /*is_raw_key*/);
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(false, encrypt_response_or);

  auto decrypt_request = CreateHpkeDecryptRequest(
      encrypt_response_or->encrypted_data().ciphertext(),
      false /*is_bidirectional*/, false /*is_raw_key*/,
      encrypt_response_or->secret());
  AssertHpkeDecryptResponse(client_->HpkeDecryptSync(decrypt_request),
                            encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest,
       HpkeEncryptUsingExternalInterfaceAndDecryptUsingSubtleInterface) {
  HpkeEncryptRequest encrypt_request = CreateHpkeEncryptRequest(false, true);
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(false, encrypt_response_or);

  auto key =
      CreateHpkePrivateKeySet(crypto::tink::HpkeKem::DHKEM_X25519_HKDF_SHA256,
                              crypto::tink::HpkeKdf::HKDF_SHA256,
                              crypto::tink::HpkeAead::CHACHA20_POLY1305);
  auto encoded_key = *Base64Encode(key.SerializeAsString());
  HpkeDecryptRequest decrypt_request;
  decrypt_request.mutable_encrypted_data()->set_key_id(string(kKeyId));
  decrypt_request.set_tink_key_binary(encoded_key);
  decrypt_request.set_shared_info(kSharedInfo);
  decrypt_request.mutable_encrypted_data()->set_ciphertext(
      encrypt_response_or->encrypted_data().ciphertext());
  AssertHpkeDecryptResponse(client_->HpkeDecryptSync(decrypt_request),
                            encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest, HpkeEncryptAndDecryptSuccessForEciesKeys) {
  auto encrypt_request = CreateHpkeEncryptRequestWithEciesKey();
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(false, encrypt_response_or);

  auto decrypt_request = CreateHpkeDecryptRequestWithEciesKey(
      encrypt_response_or->encrypted_data().ciphertext());
  AssertHpkeDecryptResponse(client_->HpkeDecryptSync(decrypt_request),
                            encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest,
       HpkeEncryptAndDecryptSuccessForInputHpkeParams) {
  HpkeParams hpke_params_from_request;
  hpke_params_from_request.set_aead(HpkeAead::CHACHA20_POLY1305);
  auto encrypt_request = CreateHpkeEncryptRequest(
      false /*is_bidirectional*/, false /*is_raw_key*/,
      "" /*encoded_public_key*/, "" /*exporter_context*/,
      hpke_params_from_request);
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(false, encrypt_response_or);

  auto decrypt_request = CreateHpkeDecryptRequest(
      encrypt_response_or->encrypted_data().ciphertext(),
      false /*is_bidirectional*/, false /*is_raw_key*/,
      encrypt_response_or->secret(), SuccessExecutionResult(),
      "" /*exporter_context*/, hpke_params_from_request);
  AssertHpkeDecryptResponse(client_->HpkeDecryptSync(decrypt_request),
                            encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest, HpkeEncryptAndDecryptSuccessForTwoDirection) {
  auto encrypt_request =
      CreateHpkeEncryptRequest(true /*is_bidirectional*/, false /*is_raw_key*/);
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(true, encrypt_response_or);

  auto decrypt_request = CreateHpkeDecryptRequest(
      encrypt_response_or->encrypted_data().ciphertext(),
      true /*is_bidirectional*/, false /*is_raw_key*/,
      encrypt_response_or->secret());
  AssertHpkeDecryptResponse(client_->HpkeDecryptSync(decrypt_request),
                            encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest, HpkeEncryptAndDecryptSuccessPassingRawKey) {
  auto encrypt_request =
      CreateHpkeEncryptRequest(true /*is_bidirectional*/, true /*is_raw_key*/);
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(true, encrypt_response_or);

  auto decrypt_request = CreateHpkeDecryptRequest(
      encrypt_response_or->encrypted_data().ciphertext(),
      true /*is_bidirectional*/, true /*is_raw_key*/,
      encrypt_response_or->secret());
  AssertHpkeDecryptResponse(client_->HpkeDecryptSync(decrypt_request),
                            encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest, HpkeEncryptAndDecryptWithInputExportContext) {
  string exporter_context = "custom exporter";
  auto encrypt_request =
      CreateHpkeEncryptRequest(true /*is_bidirectional*/, false /*is_raw_key*/,
                               "" /*encoded_public_key*/, exporter_context);
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(true, encrypt_response_or);

  auto decrypt_request = CreateHpkeDecryptRequest(
      encrypt_response_or->encrypted_data().ciphertext(),
      true /*is_bidirectional*/, false /*is_raw_key*/,
      encrypt_response_or->secret(), SuccessExecutionResult(),
      exporter_context);
  AssertHpkeDecryptResponse(client_->HpkeDecryptSync(decrypt_request),
                            encrypt_response_or->secret());
}

TEST_F(CryptoClientProviderTest, HpkeEncryptFailedToDecodePrivateKey) {
  auto request =
      CreateHpkeEncryptRequest(false /*is_bidirectional*/, false /*is_raw_key*/,
                               "abc" /*encoded_public_key*/);
  AssertHpkeEncryptResponse(
      false, client_->HpkeEncryptSync(request),
      FailureExecutionResult(SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH));
}

TEST_F(CryptoClientProviderTest, HpkeEncryptFailedToCreateCipher) {
  auto request =
      CreateHpkeEncryptRequest(false /*is_bidirectional*/, true /*is_raw_key*/,
                               "abcd" /*encoded_public_key*/);
  AssertHpkeEncryptResponse(
      false, client_->HpkeEncryptSync(request),
      FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED));
}

TEST_F(CryptoClientProviderTest, HpkeEncryptFailedToCreateKeyset) {
  auto request =
      CreateHpkeEncryptRequest(false /*is_bidirectional*/, false /*is_raw_key*/,
                               "abcd" /*encoded_public_key*/);
  AssertHpkeEncryptResponse(
      false, client_->HpkeEncryptSync(request),
      FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE));
}

TEST_F(CryptoClientProviderTest, HpkeDecryptFailedToCreateKeySet) {
  auto encrypt_request = CreateHpkeEncryptRequest(false /*is_bidirectional*/,
                                                  false /*is_raw_key*/);
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(false, encrypt_response_or);

  auto decrypt_request = CreateHpkeDecryptRequest(
      encrypt_response_or->encrypted_data().ciphertext(),
      false /*is_bidirectional*/, false /*is_raw_key*/,
      encrypt_response_or->secret(), FailureExecutionResult(SC_UNKNOWN));
  AssertHpkeDecryptResponse(
      client_->HpkeDecryptSync(decrypt_request), encrypt_response_or->secret(),
      FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE));
}

TEST_F(CryptoClientProviderTest,
       HpkeEncryptFailedToReadKeysetForBiDirectional) {
  auto request =
      CreateHpkeEncryptRequest(true /*is_bidirectional*/, false /*is_raw_key*/,
                               "abcd" /*encoded_public_key*/);
  AssertHpkeEncryptResponse(
      false, client_->HpkeEncryptSync(request),
      FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_READ_KEYSET_FAILED));
}

TEST_F(CryptoClientProviderTest,
       HpkeDecryptFailedToReadKeySetForBiDirectional) {
  auto encrypt_request = CreateHpkeEncryptRequest(false /*is_bidirectional*/,
                                                  false /*is_raw_key*/);
  auto encrypt_response_or = client_->HpkeEncryptSync(encrypt_request);
  AssertHpkeEncryptResponse(false, encrypt_response_or);

  auto decrypt_request = CreateHpkeDecryptRequest(
      encrypt_response_or->encrypted_data().ciphertext(),
      true /*is_bidirectional*/, false /*is_raw_key*/,
      encrypt_response_or->secret(), FailureExecutionResult(SC_UNKNOWN));
  AssertHpkeDecryptResponse(
      client_->HpkeDecryptSync(decrypt_request), encrypt_response_or->secret(),
      FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_READ_KEYSET_FAILED));
}

TEST_F(CryptoClientProviderTest, AeadEncryptAndDecryptSuccessFor128Secret) {
  auto encrypt_request = CreateAeadEncryptRequest(kSecret128);
  auto encrypt_response_or = client_->AeadEncryptSync(encrypt_request);
  EXPECT_SUCCESS(encrypt_response_or.result());

  auto ciphertext = encrypt_response_or->encrypted_data().ciphertext();
  auto decrypt_request = CreateAeadDecryptRequest(kSecret128, ciphertext);
  auto decrypt_response_or = client_->AeadDecryptSync(decrypt_request);
  EXPECT_SUCCESS(decrypt_response_or.result());
  EXPECT_EQ(decrypt_response_or->payload(), kPayload);
}

TEST_F(CryptoClientProviderTest, AeadEncryptAndDecryptSuccessFor256Secret) {
  auto encrypt_request = CreateAeadEncryptRequest(kSecret256);
  auto encrypt_response_or = client_->AeadEncryptSync(encrypt_request);
  EXPECT_SUCCESS(encrypt_response_or.result());

  auto ciphertext = encrypt_response_or->encrypted_data().ciphertext();
  auto decrypt_request = CreateAeadDecryptRequest(kSecret256, ciphertext);
  auto decrypt_response_or = client_->AeadDecryptSync(decrypt_request);
  EXPECT_SUCCESS(decrypt_response_or.result());
  EXPECT_EQ(decrypt_response_or->payload(), kPayload);
}

TEST_F(CryptoClientProviderTest, CannotCreateAeadDueToInvalidSecret) {
  SecretData invalid_secret(4, 'x');
  string secret_str(invalid_secret.begin(), invalid_secret.end());
  auto encrypt_request = CreateAeadEncryptRequest(secret_str);
  EXPECT_THAT(client_->AeadEncryptSync(encrypt_request).result(),
              ResultIs(FailureExecutionResult(
                  SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED)));

  auto decrypt_request = CreateAeadDecryptRequest(secret_str, kPayload);
  EXPECT_THAT(client_->AeadDecryptSync(decrypt_request).result(),
              ResultIs(FailureExecutionResult(
                  SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED)));
}

TEST_F(CryptoClientProviderTest, ComputeMacFailedDueToMissingKey) {
  ComputeMacRequest request;
  string data = "some sensitive data";
  request.set_data(data);

  EXPECT_THAT(client_->ComputeMacSync(move(request)).result(),
              FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_MISSING_KEY));
}

TEST_F(CryptoClientProviderTest, ComputeMacFailedDueToMissingData) {
  ComputeMacRequest request;
  string key = "some key";
  request.set_key(key);

  EXPECT_THAT(client_->ComputeMacSync(move(request)).result(),
              FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_MISSING_DATA));
}

TEST_F(CryptoClientProviderTest, ComputeMacSuccessfully) {
  auto keyset_handle = KeysetHandle::GenerateNew(MacKeyTemplates::HmacSha256());
  std::stringbuf key_buf(std::ios_base::out);
  auto keyset_writer =
      BinaryKeysetWriter::New(std::make_unique<std::ostream>(&key_buf));
  auto write_result = CleartextKeysetHandle::Write(keyset_writer->get(),
                                                   *keyset_handle->release());
  EXPECT_TRUE(write_result.ok()) << write_result;

  ComputeMacRequest request;
  ASSERT_SUCCESS_AND_ASSIGN(*request.mutable_key(),
                            Base64Encode(key_buf.str()));
  string data = "some sensitive data";
  request.set_data(data);
  EXPECT_THAT(client_->ComputeMacSync(move(request))->mac(),
              testing::Not(testing::IsEmpty()));
}

StreamingAeadParams ValidAesGcmHkdfParams(bool is_binary) {
  AesGcmHkdfStreamingKey key;
  key.set_version(0);
  key.set_key_value(
      *Base64Encode(HexStringToBytes(kDecryptedPrivateKeyForAes128Gcm)));
  key.mutable_params()->set_derived_key_size(16);
  key.mutable_params()->set_hkdf_hash_type(
      google::crypto::tink::HashType::SHA256);
  key.mutable_params()->set_ciphertext_segment_size(1024);
  StreamingAeadParams params;
  if (is_binary) {
    Keyset keyset;
    keyset.set_primary_key_id(123);
    keyset.add_key();
    keyset.mutable_key(0)->set_key_id(456);
    keyset.mutable_key(0)->mutable_key_data()->set_value(
        key.SerializeAsString());
    auto encoded_key = *Base64Encode(keyset.SerializeAsString());
    params.mutable_aes_gcm_hkdf_key()->set_tink_key_binary(encoded_key);
  } else {
    params.mutable_aes_gcm_hkdf_key()
        ->mutable_raw_key_with_params()
        ->set_version(0);
    params.mutable_aes_gcm_hkdf_key()
        ->mutable_raw_key_with_params()
        ->set_key_value(
            *Base64Encode(HexStringToBytes(kDecryptedPrivateKeyForAes128Gcm)));
    params.mutable_aes_gcm_hkdf_key()
        ->mutable_raw_key_with_params()
        ->mutable_params()
        ->set_derived_key_size(16);
    params.mutable_aes_gcm_hkdf_key()
        ->mutable_raw_key_with_params()
        ->mutable_params()
        ->set_hkdf_hash_type(HashType::SHA256);
    params.mutable_aes_gcm_hkdf_key()
        ->mutable_raw_key_with_params()
        ->mutable_params()
        ->set_ciphertext_segment_size(1024);
  }
  params.set_shared_info(kSharedInfo);
  return params;
}

StreamingAeadParams ValidAesCtrHmacParams(bool is_binary) {
  AesCtrHmacStreamingKey key;
  key.set_version(0);
  key.set_key_value(
      *Base64Encode(HexStringToBytes(kDecryptedPrivateKeyForAes128Gcm)));
  key.mutable_params()->set_derived_key_size(16);
  key.mutable_params()->set_hkdf_hash_type(
      google::crypto::tink::HashType::SHA256);
  key.mutable_params()->set_ciphertext_segment_size(1024);
  key.mutable_params()->mutable_hmac_params()->set_hash(
      google::crypto::tink::HashType::SHA256);
  key.mutable_params()->mutable_hmac_params()->set_tag_size(32);
  StreamingAeadParams params;
  if (is_binary) {
    Keyset keyset;
    keyset.set_primary_key_id(123);
    keyset.add_key();
    keyset.mutable_key(0)->set_key_id(456);
    keyset.mutable_key(0)->mutable_key_data()->set_value(
        key.SerializeAsString());
    auto encoded_key = *Base64Encode(keyset.SerializeAsString());
    params.mutable_aes_ctr_hmac_key()->set_tink_key_binary(encoded_key);
  } else {
    params.mutable_aes_ctr_hmac_key()
        ->mutable_raw_key_with_params()
        ->set_version(0);
    params.mutable_aes_ctr_hmac_key()
        ->mutable_raw_key_with_params()
        ->set_key_value(
            *Base64Encode(HexStringToBytes(kDecryptedPrivateKeyForAes128Gcm)));
    params.mutable_aes_ctr_hmac_key()
        ->mutable_raw_key_with_params()
        ->mutable_params()
        ->set_derived_key_size(16);
    params.mutable_aes_ctr_hmac_key()
        ->mutable_raw_key_with_params()
        ->mutable_params()
        ->set_hkdf_hash_type(HashType::SHA256);
    params.mutable_aes_ctr_hmac_key()
        ->mutable_raw_key_with_params()
        ->mutable_params()
        ->set_ciphertext_segment_size(1024);
    params.mutable_aes_ctr_hmac_key()
        ->mutable_raw_key_with_params()
        ->mutable_params()
        ->mutable_hmac_params()
        ->set_hash(HashType::SHA256);
    params.mutable_aes_ctr_hmac_key()
        ->mutable_raw_key_with_params()
        ->mutable_params()
        ->mutable_hmac_params()
        ->set_tag_size(32);
  }
  params.set_shared_info(kSharedInfo);
  return params;
}

Status WriteToStream(OutputStream* output_stream, string contents,
                     bool close_stream) {
  void* buffer;
  int pos = 0;
  int remaining = contents.length();
  int available_space = 0;
  int available_bytes = 0;
  while (remaining > 0) {
    auto next_result = output_stream->Next(&buffer);
    if (!next_result.ok()) return next_result.status();
    available_space = next_result.value();
    available_bytes = min(available_space, remaining);
    memcpy(buffer, contents.data() + pos, available_bytes);
    remaining -= available_bytes;
    pos += available_bytes;
  }
  if (available_space > available_bytes) {
    output_stream->BackUp(available_space - available_bytes);
  }
  return close_stream ? output_stream->Close() : OkStatus();
}

Status ReadFromStream(InputStream* input_stream, std::string* output) {
  if (input_stream == nullptr || output == nullptr) {
    return Status(absl::StatusCode::kInternal, "Illegal read from a stream");
  }
  const void* buffer;
  output->clear();
  while (true) {
    auto next_result = input_stream->Next(&buffer);
    if (next_result.status().code() == absl::StatusCode::kOutOfRange) {
      // End of stream.
      return OkStatus();
    }
    if (!next_result.ok()) return next_result.status();
    auto read_bytes = next_result.value();
    if (read_bytes > 0) {
      output->append(string(reinterpret_cast<const char*>(buffer), read_bytes));
    }
  }
  return OkStatus();
}

TEST_F(CryptoClientProviderTest,
       AeadStreamEncryptAndDecryptSuccessForAesCtrHmacRawKey) {
  // Encrypt
  auto params = ValidAesCtrHmacParams(false);
  AeadEncryptStreamRequest encrypt_request;
  encrypt_request.saead_params = params;
  encrypt_request.ciphertext_stream = make_unique<std::stringstream>();
  auto enc_stream_result = client_->AeadEncryptStreamSync(encrypt_request);
  EXPECT_SUCCESS(enc_stream_result);
  string plaintext = Random::GetRandomBytes(10000);
  auto status = WriteToStream(enc_stream_result.value().get(), plaintext, true);
  EXPECT_TRUE(status.ok());

  // Decrypt
  AeadDecryptStreamRequest decrypt_request;
  decrypt_request.saead_params = params;
  stringstream ss;
  ss << encrypt_request.ciphertext_stream->rdbuf();
  decrypt_request.ciphertext_stream = make_unique<stringstream>(ss.str());
  auto dec_stream_result = client_->AeadDecryptStreamSync(decrypt_request);
  EXPECT_SUCCESS(dec_stream_result);
  std::string decrypted;
  status = ReadFromStream(dec_stream_result.value().get(), &decrypted);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(CryptoClientProviderTest,
       AeadStreamEncryptAndDecryptSuccessForAesGcmHkdfRawKey) {
  // Encrypt
  auto params = ValidAesGcmHkdfParams(false);
  AeadEncryptStreamRequest encrypt_request;
  encrypt_request.saead_params = params;
  encrypt_request.ciphertext_stream = make_unique<std::stringstream>();
  auto enc_stream_result = client_->AeadEncryptStreamSync(encrypt_request);
  EXPECT_SUCCESS(enc_stream_result);
  string plaintext = Random::GetRandomBytes(10000);

  auto status = WriteToStream(enc_stream_result.value().get(), plaintext, true);
  EXPECT_TRUE(status.ok());

  // Decrypt
  AeadDecryptStreamRequest decrypt_request;
  decrypt_request.saead_params = params;
  stringstream ss;
  ss << encrypt_request.ciphertext_stream->rdbuf();
  decrypt_request.ciphertext_stream = make_unique<stringstream>(ss.str());
  auto dec_stream_result = client_->AeadDecryptStreamSync(decrypt_request);
  EXPECT_SUCCESS(dec_stream_result);
  std::string decrypted;
  status = ReadFromStream(dec_stream_result.value().get(), &decrypted);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(CryptoClientProviderTest,
       AeadStreamEncryptAndDecryptSuccessForAesCtrHmacBinaryKey) {
  // Encrypt
  auto params = ValidAesCtrHmacParams(true);
  AeadEncryptStreamRequest encrypt_request;
  encrypt_request.saead_params = params;
  encrypt_request.ciphertext_stream = make_unique<std::stringstream>();
  auto enc_stream_result = client_->AeadEncryptStreamSync(encrypt_request);
  EXPECT_SUCCESS(enc_stream_result);
  string plaintext = Random::GetRandomBytes(10000);
  auto status = WriteToStream(enc_stream_result.value().get(), plaintext, true);
  EXPECT_TRUE(status.ok());

  // Decrypt
  AeadDecryptStreamRequest decrypt_request;
  decrypt_request.saead_params = params;
  stringstream ss;
  ss << encrypt_request.ciphertext_stream->rdbuf();
  decrypt_request.ciphertext_stream = make_unique<stringstream>(ss.str());
  auto dec_stream_result = client_->AeadDecryptStreamSync(decrypt_request);
  EXPECT_SUCCESS(dec_stream_result);
  std::string decrypted;
  status = ReadFromStream(dec_stream_result.value().get(), &decrypted);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(CryptoClientProviderTest,
       AeadStreamEncryptAndDecryptSuccessForAesGcmHkdfBinaryKey) {
  // Encrypt
  auto params = ValidAesGcmHkdfParams(true);
  AeadEncryptStreamRequest encrypt_request;
  encrypt_request.saead_params = params;
  encrypt_request.ciphertext_stream = make_unique<std::stringstream>();
  auto enc_stream_result = client_->AeadEncryptStreamSync(encrypt_request);
  EXPECT_SUCCESS(enc_stream_result);
  string plaintext = Random::GetRandomBytes(10000);

  auto status = WriteToStream(enc_stream_result.value().get(), plaintext, true);
  EXPECT_TRUE(status.ok());

  // Decrypt
  AeadDecryptStreamRequest decrypt_request;
  decrypt_request.saead_params = params;
  stringstream ss;
  ss << encrypt_request.ciphertext_stream->rdbuf();
  decrypt_request.ciphertext_stream = make_unique<stringstream>(ss.str());
  auto dec_stream_result = client_->AeadDecryptStreamSync(decrypt_request);
  EXPECT_SUCCESS(dec_stream_result);
  std::string decrypted;
  status = ReadFromStream(dec_stream_result.value().get(), &decrypted);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(plaintext, decrypted);
}
}  // namespace google::scp::cpio::client_providers::test
