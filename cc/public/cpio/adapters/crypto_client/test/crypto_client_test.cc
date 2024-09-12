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
// WITHOUT WARRANTIES OR finishedS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "public/cpio/adapters/crypto_client/src/crypto_client.h"

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <utility>

#include "cc/core/test/utils/proto_test_utils.h"
#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/crypto_client/mock/mock_crypto_client_with_overrides.h"
#include "public/cpio/interface/crypto_client/crypto_client_interface.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/interface/error_codes.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

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
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CPIO_UNKNOWN_ERROR;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::CryptoClient;
using google::scp::cpio::CryptoClientOptions;
using google::scp::cpio::mock::MockCryptoClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Return;

namespace google::scp::cpio::test {
class CryptoClientTest : public ScpTestBase {
 protected:
  CryptoClientTest() {
    auto crypto_client_options = make_shared<CryptoClientOptions>();
    client_ = make_unique<MockCryptoClientWithOverrides>(crypto_client_options);

    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());
  }

  ~CryptoClientTest() { EXPECT_SUCCESS(client_->Stop()); }

  unique_ptr<MockCryptoClientWithOverrides> client_;
};

TEST_F(CryptoClientTest, HpkeEncryptSyncSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), HpkeEncryptSync)
      .WillOnce(Return(HpkeEncryptResponse()));

  EXPECT_THAT(client_->HpkeEncryptSync(HpkeEncryptRequest()),
              IsSuccessfulAndHolds(EqualsProto(HpkeEncryptResponse())));
}

TEST_F(CryptoClientTest, HpkeEncryptSyncFailure) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  EXPECT_CALL(*client_->GetCryptoClientProvider(), HpkeEncryptSync)
      .WillOnce(Return(failure));

  EXPECT_THAT(client_->HpkeEncryptSync(HpkeEncryptRequest()).result(),
              ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}

TEST_F(CryptoClientTest, HpkeDecryptSyncSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), HpkeDecryptSync)
      .WillOnce(Return(HpkeDecryptResponse()));

  EXPECT_THAT(client_->HpkeDecryptSync(HpkeDecryptRequest()),
              IsSuccessfulAndHolds(EqualsProto(HpkeDecryptResponse())));
}

TEST_F(CryptoClientTest, HpkeDecryptSyncFailure) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  EXPECT_CALL(*client_->GetCryptoClientProvider(), HpkeDecryptSync)
      .WillOnce(Return(failure));

  EXPECT_THAT(client_->HpkeDecryptSync(HpkeDecryptRequest()).result(),
              ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}

TEST_F(CryptoClientTest, AeadEncryptSyncSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), AeadEncryptSync)
      .WillOnce(Return(AeadEncryptResponse()));

  EXPECT_THAT(client_->AeadEncryptSync(AeadEncryptRequest()),
              IsSuccessfulAndHolds(EqualsProto(AeadEncryptResponse())));
}

TEST_F(CryptoClientTest, AeadEncryptSyncFailure) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  EXPECT_CALL(*client_->GetCryptoClientProvider(), AeadEncryptSync)
      .WillOnce(Return(failure));

  EXPECT_THAT(client_->AeadEncryptSync(AeadEncryptRequest()).result(),
              ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}

TEST_F(CryptoClientTest, AeadDecryptSyncSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), AeadDecryptSync)
      .WillOnce(Return(AeadDecryptResponse()));

  EXPECT_THAT(client_->AeadDecryptSync(AeadDecryptRequest()),
              IsSuccessfulAndHolds(EqualsProto(AeadDecryptResponse())));
}

TEST_F(CryptoClientTest, AeadDecryptSyncFailure) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  EXPECT_CALL(*client_->GetCryptoClientProvider(), AeadDecryptSync)
      .WillOnce(Return(failure));

  EXPECT_THAT(client_->AeadDecryptSync(AeadDecryptRequest()).result(),
              ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}

TEST_F(CryptoClientTest, ComputeMacSyncSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), ComputeMacSync)
      .WillOnce(Return(ComputeMacResponse()));

  EXPECT_THAT(client_->ComputeMacSync(ComputeMacRequest()),
              IsSuccessfulAndHolds(EqualsProto(ComputeMacResponse())));
}

TEST_F(CryptoClientTest, ComputeMacSyncFailure) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  EXPECT_CALL(*client_->GetCryptoClientProvider(), ComputeMacSync)
      .WillOnce(Return(failure));

  EXPECT_THAT(client_->ComputeMacSync(ComputeMacRequest()).result(),
              ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}
}  // namespace google::scp::cpio::test
