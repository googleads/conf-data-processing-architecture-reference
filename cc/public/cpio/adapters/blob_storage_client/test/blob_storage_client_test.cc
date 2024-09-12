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

#include "public/cpio/adapters/blob_storage_client/src/blob_storage_client.h"

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/blob_storage_client/mock/mock_blob_storage_client_with_overrides.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "public/cpio/interface/error_codes.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

using google::cmrt::sdk::blob_storage_service::v1::BlobMetadata;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CPIO_UNKNOWN_ERROR;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::mock::MockBlobStorageClientWithOverrides;
using std::atomic_bool;
using std::make_shared;
using testing::Return;

namespace google::scp::cpio::test {

class BlobStorageClientTest : public ScpTestBase {
 protected:
  BlobStorageClientTest() {
    assert(client_.Init().Successful());
    assert(client_.Run().Successful());
  }

  ~BlobStorageClientTest() { assert(client_.Stop().Successful()); }

  MockBlobStorageClientWithOverrides client_;
};

TEST_F(BlobStorageClientTest, GetBlobSuccess) {
  atomic_bool finished(false);
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlob)
      .WillOnce([](auto context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetBlobResponse>();
        context.Finish();
      });

  auto context = AsyncContext<GetBlobRequest, GetBlobResponse>(
      make_shared<GetBlobRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(GetBlobResponse()));
        finished = true;
      });
  client_.GetBlob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, GetBlobSyncSuccess) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlob)
      .WillOnce([=](AsyncContext<GetBlobRequest, GetBlobResponse>& context) {
        context.response = make_shared<GetBlobResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(client_.GetBlobSync(GetBlobRequest()).result());
}

TEST_F(BlobStorageClientTest, ListBlobsMetadataSuccess) {
  atomic_bool finished(false);
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), ListBlobsMetadata)
      .WillOnce([](auto context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<ListBlobsMetadataResponse>();
        context.Finish();
      });

  auto context =
      AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>(
          make_shared<ListBlobsMetadataRequest>(), [&finished](auto& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_THAT(*context.response,
                        EqualsProto(ListBlobsMetadataResponse()));
            finished = true;
          });
  client_.ListBlobsMetadata(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, ListBlobsMetadataSyncSuccess) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), ListBlobsMetadata)
      .WillOnce([=](AsyncContext<ListBlobsMetadataRequest,
                                 ListBlobsMetadataResponse>& context) {
        context.response = make_shared<ListBlobsMetadataResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(
      client_.ListBlobsMetadataSync(ListBlobsMetadataRequest()).result());
}

TEST_F(BlobStorageClientTest, PutBlobSuccess) {
  atomic_bool finished(false);
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlob)
      .WillOnce([](auto context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<PutBlobResponse>();
        context.Finish();
      });

  auto context = AsyncContext<PutBlobRequest, PutBlobResponse>(
      make_shared<PutBlobRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(PutBlobResponse()));
        finished = true;
      });
  client_.PutBlob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, PutBlobSyncSuccess) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlob)
      .WillOnce([=](AsyncContext<PutBlobRequest, PutBlobResponse>& context) {
        context.response = make_shared<PutBlobResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(client_.PutBlobSync(PutBlobRequest()).result());
}

TEST_F(BlobStorageClientTest, DeleteBlobSuccess) {
  atomic_bool finished(false);
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), DeleteBlob)
      .WillOnce([](auto context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<DeleteBlobResponse>();
        context.Finish();
      });

  auto context = AsyncContext<DeleteBlobRequest, DeleteBlobResponse>(
      make_shared<DeleteBlobRequest>(), [&finished](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(DeleteBlobResponse()));
        finished = true;
      });
  client_.DeleteBlob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, DeleteBlobSyncSuccess) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), DeleteBlob)
      .WillOnce(
          [=](AsyncContext<DeleteBlobRequest, DeleteBlobResponse>& context) {
            context.response = make_shared<DeleteBlobResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_.DeleteBlobSync(DeleteBlobRequest()).result());
}

TEST_F(BlobStorageClientTest, GetBlobStreamSuccess) {
  atomic_bool finished(false);
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlobStream)
      .WillOnce([](auto context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetBlobStreamResponse>();
        context.Finish();
      });

  ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse> context;
  context.request = make_shared<GetBlobStreamRequest>();
  context.process_callback = [&finished](auto& context, auto) {
    EXPECT_SUCCESS(context.result);
    EXPECT_THAT(*context.response, EqualsProto(GetBlobStreamResponse()));
    finished = true;
  };
  client_.GetBlobStream(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, PutBlobStreamSuccess) {
  atomic_bool finished(false);
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlobStream)
      .WillOnce([](auto context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<PutBlobStreamResponse>();
        context.Finish();
      });

  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse> context;
  context.request = make_shared<PutBlobStreamRequest>();
  context.callback = [&finished](auto& context) {
    EXPECT_SUCCESS(context.result);
    EXPECT_THAT(*context.response, EqualsProto(PutBlobStreamResponse()));
    finished = true;
  };
  client_.PutBlobStream(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, GetBlobFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlob)
      .WillOnce([=](AsyncContext<GetBlobRequest, GetBlobResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return FailureExecutionResult(SC_UNKNOWN);
      });

  atomic_bool finished = false;
  auto context = AsyncContext<GetBlobRequest, GetBlobResponse>(
      make_shared<GetBlobRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.GetBlob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, GetBlobSyncFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlob)
      .WillOnce([=](AsyncContext<GetBlobRequest, GetBlobResponse>& context) {
        context.result = FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR);
        context.Finish();
      });
  EXPECT_THAT(client_.GetBlobSync(GetBlobRequest()).result(),
              ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}

TEST_F(BlobStorageClientTest, PutBlobFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlob)
      .WillOnce([=](AsyncContext<PutBlobRequest, PutBlobResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return FailureExecutionResult(SC_UNKNOWN);
      });

  atomic_bool finished = false;
  auto context = AsyncContext<PutBlobRequest, PutBlobResponse>(
      make_shared<PutBlobRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.PutBlob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, PutBlobSyncFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlob)
      .WillOnce([=](AsyncContext<PutBlobRequest, PutBlobResponse>& context) {
        context.result = FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR);
        context.Finish();
      });
  EXPECT_THAT(client_.PutBlobSync(PutBlobRequest()).result(),
              ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}

TEST_F(BlobStorageClientTest, ListBlobsMetadataFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), ListBlobsMetadata)
      .WillOnce([=](AsyncContext<ListBlobsMetadataRequest,
                                 ListBlobsMetadataResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return FailureExecutionResult(SC_UNKNOWN);
      });

  atomic_bool finished = false;
  auto context =
      AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>(
          make_shared<ListBlobsMetadataRequest>(), [&](auto& context) {
            EXPECT_THAT(
                context.result,
                ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
            finished = true;
          });
  client_.ListBlobsMetadata(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, ListBlobsMetadataSyncFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), ListBlobsMetadata)
      .WillOnce([=](AsyncContext<ListBlobsMetadataRequest,
                                 ListBlobsMetadataResponse>& context) {
        context.result = FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR);
        context.Finish();
      });
  EXPECT_THAT(
      client_.ListBlobsMetadataSync(ListBlobsMetadataRequest()).result(),
      ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}

TEST_F(BlobStorageClientTest, DeleteBlobFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), DeleteBlob)
      .WillOnce(
          [=](AsyncContext<DeleteBlobRequest, DeleteBlobResponse>& context) {
            context.result = FailureExecutionResult(SC_UNKNOWN);
            context.Finish();
            return FailureExecutionResult(SC_UNKNOWN);
          });

  atomic_bool finished = false;
  auto context = AsyncContext<DeleteBlobRequest, DeleteBlobResponse>(
      make_shared<DeleteBlobRequest>(), [&](auto& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
        finished = true;
      });
  client_.DeleteBlob(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, DeleteBlobSyncFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), DeleteBlob)
      .WillOnce(
          [=](AsyncContext<DeleteBlobRequest, DeleteBlobResponse>& context) {
            context.result = FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR);
            context.Finish();
          });
  EXPECT_THAT(client_.DeleteBlobSync(DeleteBlobRequest()).result(),
              ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
}

TEST_F(BlobStorageClientTest, GetBlobStreamFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlobStream)
      .WillOnce([=](ConsumerStreamingContext<GetBlobStreamRequest,
                                             GetBlobStreamResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return FailureExecutionResult(SC_UNKNOWN);
      });

  atomic_bool finished = false;
  ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse> context;
  context.process_callback = [&finished](auto& context, bool) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
    finished = true;
  };

  client_.GetBlobStream(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(BlobStorageClientTest, PutBlobStreamFailure) {
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlobStream)
      .WillOnce([=](ProducerStreamingContext<PutBlobStreamRequest,
                                             PutBlobStreamResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });

  atomic_bool finished = false;
  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse> context;
  context.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR)));
    finished = true;
  };

  client_.PutBlobStream(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST(BlobStorageClientInitTest, FailureToRun) {
  auto blob_storage_client_options = make_shared<BlobStorageClientOptions>();
  auto client = make_unique<MockBlobStorageClientWithOverrides>(
      blob_storage_client_options);

  EXPECT_SUCCESS(client->Init());
  EXPECT_CALL(client->GetBlobStorageClientProvider(), Run)
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_EQ(client->Run(), FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR));
}

TEST(BlobStorageClientInitTest, FailureToStop) {
  auto blobS_storage_client_options = make_shared<BlobStorageClientOptions>();
  auto client = make_unique<MockBlobStorageClientWithOverrides>(
      blobS_storage_client_options);

  EXPECT_SUCCESS(client->Init());
  EXPECT_CALL(client->GetBlobStorageClientProvider(), Stop)
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_SUCCESS(client->Run());
  EXPECT_EQ(client->Stop(), FailureExecutionResult(SC_CPIO_UNKNOWN_ERROR));
}
}  // namespace google::scp::cpio::test
