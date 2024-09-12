// Copyright 2023 Google LLC
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

#include "core/nosql_database_provider/src/common/nosql_database_provider_upsert_rate_controller.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/nosql_database_provider/mock/mock_nosql_database_provider_no_overrides.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::UpsertDatabaseItemRequest;
using google::scp::core::UpsertDatabaseItemResponse;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProviderNoOverrides;
using google::scp::core::test::ResultIs;
using std::get;
using std::make_shared;
using std::shared_ptr;
using std::string;
using ::testing::Return;

namespace google::scp::core::test {

TEST(NoSQLDatabaseProviderUpsertRateControllerTest,
     ReturnsSuccessIfDoesNotExceedThreshold) {
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  EXPECT_CALL(*mock_nosql_database_provider, UpsertDatabaseItem)
      .WillRepeatedly([](auto& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<UpsertDatabaseItemResponse>();
        context.callback(context);
        return SuccessExecutionResult();
      });
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      mock_nosql_database_provider;
  NoSQLDatabaseProviderUpsertRateController rate_controller(
      nosql_database_provider, /*max_outstanding_upsert_operations_count=*/1);

  // Multiple requests can go through..
  for (int i = 0; i < 100; i++) {
    std::atomic<bool> completed = false;
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse> context;
    context.request = make_shared<UpsertDatabaseItemRequest>();
    context.callback = [&completed](auto& context) { completed = true; };
    EXPECT_SUCCESS(rate_controller.UpsertDatabaseItem(context));
    EXPECT_TRUE(completed);
  }
}

TEST(NoSQLDatabaseProviderUpsertRateControllerTest,
     ReturnsSuccessIfMeetsThreshold) {
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  EXPECT_CALL(*mock_nosql_database_provider, UpsertDatabaseItem)
      .WillRepeatedly([](auto& context) {
        // Do not complete the callback of the context
        return SuccessExecutionResult();
      });
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      mock_nosql_database_provider;
  NoSQLDatabaseProviderUpsertRateController rate_controller(
      nosql_database_provider, /*max_outstanding_upsert_operations_count=*/2);
  {
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse> context;
    context.request = make_shared<UpsertDatabaseItemRequest>();
    context.callback = [](auto& context) {};
    EXPECT_SUCCESS(rate_controller.UpsertDatabaseItem(context));
  }
  {
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse> context;
    context.request = make_shared<UpsertDatabaseItemRequest>();
    context.callback = [](auto& context) {};
    EXPECT_SUCCESS(rate_controller.UpsertDatabaseItem(context));
  }
}

TEST(NoSQLDatabaseProviderUpsertRateControllerTest,
     ReturnsRetryIfExceedsThreshold) {
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  EXPECT_CALL(*mock_nosql_database_provider, UpsertDatabaseItem)
      .WillRepeatedly([](auto& context) {
        // Do not complete the callback of the context
        return SuccessExecutionResult();
      });
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      mock_nosql_database_provider;
  NoSQLDatabaseProviderUpsertRateController rate_controller(
      nosql_database_provider, /*max_outstanding_upsert_operations_count=*/1);
  {
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse> context;
    context.request = make_shared<UpsertDatabaseItemRequest>();
    context.callback = [](auto& context) {};
    EXPECT_SUCCESS(rate_controller.UpsertDatabaseItem(context));
  }
  {
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse> context;
    context.request = make_shared<UpsertDatabaseItemRequest>();
    context.callback = [](auto& context) {};
    EXPECT_THAT(
        rate_controller.UpsertDatabaseItem(context),
        ResultIs(RetryExecutionResult(
            errors::SC_NO_SQL_DATABASE_PROVIDER_UPSERT_OPERATION_THROTTLED)));
  }
}

TEST(NoSQLDatabaseProviderUpsertRateControllerTest,
     PassesTheResultAndResponseBack) {
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  EXPECT_CALL(*mock_nosql_database_provider, UpsertDatabaseItem)
      .WillRepeatedly([](auto& context) {
        context.response = make_shared<UpsertDatabaseItemResponse>();
        context.result = FailureExecutionResult(123);
        context.callback(context);
        return SuccessExecutionResult();
      });
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      mock_nosql_database_provider;

  NoSQLDatabaseProviderUpsertRateController rate_controller(
      nosql_database_provider, /*max_outstanding_upsert_operations_count=*/1);
  std::atomic<bool> completed = false;
  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse> context;
  context.request = make_shared<UpsertDatabaseItemRequest>();
  context.callback = [&completed](auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(123)));
    completed = true;
  };
  EXPECT_SUCCESS(rate_controller.UpsertDatabaseItem(context));
  EXPECT_TRUE(completed);
}
}  // namespace google::scp::core::test
