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

#include "public/cpio/adapters/nosql_database_client/src/nosql_database_client.h"

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/nosql_database_client/mock/mock_nosql_database_client_with_overrides.h"
#include "public/cpio/interface/nosql_database_client/nosql_database_client_interface.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

using google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::CreateTableRequest;
using google::cmrt::sdk::nosql_database_service::v1::CreateTableResponse;
using google::cmrt::sdk::nosql_database_service::v1::DeleteTableRequest;
using google::cmrt::sdk::nosql_database_service::v1::DeleteTableResponse;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse;
using google::scp::core::AsyncContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::mock::MockNoSQLDatabaseClientWithOverrides;
using std::atomic;
using std::make_shared;
using testing::Return;

namespace google::scp::cpio::test {

class NoSQLDatabaseClientTest : public ScpTestBase {
 protected:
  NoSQLDatabaseClientTest() { assert(client_.Init().Successful()); }

  MockNoSQLDatabaseClientWithOverrides client_;
};

TEST_F(NoSQLDatabaseClientTest, CreateTableSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), CreateTable)
      .WillOnce([=](auto& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<CreateTableResponse>();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<CreateTableRequest, CreateTableResponse>(
      make_shared<CreateTableRequest>(), [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(CreateTableResponse()));
        finished = true;
      });

  client_.CreateTable(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(NoSQLDatabaseClientTest, CreateTableSyncSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), CreateTable)
      .WillOnce(
          [=](AsyncContext<CreateTableRequest, CreateTableResponse>& context) {
            context.response = make_shared<CreateTableResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_.CreateTableSync(CreateTableRequest()).result());
}

TEST_F(NoSQLDatabaseClientTest, DeleteTableSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), DeleteTable)
      .WillOnce([=](auto& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<DeleteTableResponse>();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<DeleteTableRequest, DeleteTableResponse>(
      make_shared<DeleteTableRequest>(), [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(DeleteTableResponse()));
        finished = true;
      });

  client_.DeleteTable(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(NoSQLDatabaseClientTest, DeleteTableSyncSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), DeleteTable)
      .WillOnce(
          [=](AsyncContext<DeleteTableRequest, DeleteTableResponse>& context) {
            context.response = make_shared<DeleteTableResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });
  EXPECT_SUCCESS(client_.DeleteTableSync(DeleteTableRequest()).result());
}

TEST_F(NoSQLDatabaseClientTest, GetDatabaseItemSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), GetDatabaseItem)
      .WillOnce([=](auto& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetDatabaseItemResponse>();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context = AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>(
      make_shared<GetDatabaseItemRequest>(), [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(*context.response, EqualsProto(GetDatabaseItemResponse()));
        finished = true;
      });

  client_.GetDatabaseItem(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(NoSQLDatabaseClientTest, GetDatabaseItemSyncSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), GetDatabaseItem)
      .WillOnce([=](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = make_shared<GetDatabaseItemResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(
      client_.GetDatabaseItemSync(GetDatabaseItemRequest()).result());
}

TEST_F(NoSQLDatabaseClientTest, CreateDatabaseItemSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), CreateDatabaseItem)
      .WillOnce([=](auto& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<CreateDatabaseItemResponse>();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context =
      AsyncContext<CreateDatabaseItemRequest, CreateDatabaseItemResponse>(
          make_shared<CreateDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_THAT(*context.response,
                        EqualsProto(CreateDatabaseItemResponse()));
            finished = true;
          });

  client_.CreateDatabaseItem(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(NoSQLDatabaseClientTest, CreateDatabaseItemSyncSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), CreateDatabaseItem)
      .WillOnce([=](AsyncContext<CreateDatabaseItemRequest,
                                 CreateDatabaseItemResponse>& context) {
        context.response = make_shared<CreateDatabaseItemResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(
      client_.CreateDatabaseItemSync(CreateDatabaseItemRequest()).result());
}

TEST_F(NoSQLDatabaseClientTest, UpsertDatabaseItemSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), UpsertDatabaseItem)
      .WillOnce([=](auto& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<UpsertDatabaseItemResponse>();
        context.Finish();
      });

  atomic<bool> finished = false;
  auto context =
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>(
          make_shared<UpsertDatabaseItemRequest>(), [&](auto& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_THAT(*context.response,
                        EqualsProto(UpsertDatabaseItemResponse()));
            finished = true;
          });

  client_.UpsertDatabaseItem(context);
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(NoSQLDatabaseClientTest, UpsertDatabaseItemSyncSuccess) {
  EXPECT_CALL(client_.GetNoSQLDatabaseClientProvider(), UpsertDatabaseItem)
      .WillOnce([=](AsyncContext<UpsertDatabaseItemRequest,
                                 UpsertDatabaseItemResponse>& context) {
        context.response = make_shared<UpsertDatabaseItemResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  EXPECT_SUCCESS(
      client_.UpsertDatabaseItemSync(UpsertDatabaseItemRequest()).result());
}
}  // namespace google::scp::cpio::test
