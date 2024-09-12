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

#include "cpio/client_providers/instance_database_client_provider/src/gcp/gcp_instance_database_client_provider.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/timestamp_test_utils.h"
#include "cpio/client_providers/common/mock/gcp/mock_gcp_database_factory.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/instance_database_client_provider/src/common/error_codes.h"
#include "cpio/common/src/gcp/error_codes.h"
#include "cpio/proto/instance_database_client.pb.h"
#include "google/cloud/spanner/admin/mocks/mock_database_admin_connection.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/status.h"
#include "google/spanner/v1/spanner.pb.h"
#include "operator/protos/shared/backend/asginstance/instance_status.pb.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::cloud::make_ready_future;
using google::cloud::Options;
using google::cloud::Status;
using google::cloud::StatusOr;
using google::cloud::spanner::Client;
using google::cloud::spanner::CommitResult;
using google::cloud::spanner::Json;
using google::cloud::spanner::MakeTimestamp;
using google::cloud::spanner::MakeUpdateMutation;
using google::cloud::spanner::Mutation;
using google::cloud::spanner::Row;
using google::cloud::spanner::RowStream;
using google::cloud::spanner::SqlStatement;
using google::cloud::spanner::Value;
using google::cloud::spanner_admin::DatabaseAdminClient;
using google::cloud::spanner_admin_mocks::MockDatabaseAdminConnection;
using google::cloud::spanner_mocks::MakeRow;
using google::cloud::spanner_mocks::MockConnection;
using google::cloud::spanner_mocks::MockResultSetSource;
using google::cmrt::sdk::instance_database_client::GetInstanceByNameRequest;
using google::cmrt::sdk::instance_database_client::GetInstanceByNameResponse;
using google::cmrt::sdk::instance_database_client::ListInstancesByStatusRequest;
using google::cmrt::sdk::instance_database_client::
    ListInstancesByStatusResponse;
using google::cmrt::sdk::instance_database_client::UpdateInstanceRequest;
using google::cmrt::sdk::instance_database_client::UpdateInstanceResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_GCP_UNKNOWN;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INVALID_INSTANCE_STATUS;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND;
using google::scp::core::errors::
    SCP_INSTANCE_DATABASE_CLIENT_PROVIDER_COMMIT_FAILED;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::MakeProtoTimestamp;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockGcpDatabaseFactory;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::operators::protos::shared::backend::asginstance::AsgInstance;
using google::scp::operators::protos::shared::backend::asginstance::
    InstanceStatus;
using google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata;
using google::spanner::admin::database::v1::UpdateDatabaseDdlRequest;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::move;
using std::optional;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::FieldsAre;
using testing::IsEmpty;
using testing::NiceMock;
using testing::NotNull;
using testing::PrintToString;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {
constexpr char kInstanceResourceName[] =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr char kSpannerInstanceName[] = "spanner";
constexpr char kSpannerDatabaseName[] = "database";
constexpr char kInstanceTableName[] = "Instance";

constexpr char kInstanceName[] =
    "https://compute.googleapis.com/projects/123456789/zones/us-central1-c/"
    "instances/987654321";

constexpr char kExpectGetInstanceQuery[] =
    "SELECT InstanceName, Status, RequestTime, TerminationTime, TTL FROM "
    "`Instance` WHERE InstanceName = "
    "'https://compute.googleapis.com/projects/123456789/zones/us-central1-c/"
    "instances/987654321'";

constexpr char kExpectListInstancesQuery[] =
    "SELECT InstanceName, Status, RequestTime, TerminationTime, TTL FROM "
    "`Instance` WHERE Status = 'TERMINATING_WAIT'";
}  // namespace

namespace google::scp::cpio::client_providers::test {

class GcpInstanceDatabaseClientProviderTests : public testing::Test {
 protected:
  GcpInstanceDatabaseClientProviderTests()
      : instance_client_(make_shared<NiceMock<MockInstanceClientProvider>>()),
        connection_(make_shared<NiceMock<MockConnection>>()),
        gcp_database_factory_(make_shared<NiceMock<MockGcpDatabaseFactory>>(
            make_shared<DatabaseClientOptions>())),
        instance_database_client_(
            make_shared<InstanceDatabaseClientOptions>(
                kSpannerInstanceName, kSpannerDatabaseName, kInstanceTableName),
            instance_client_, make_shared<MockAsyncExecutor>(),
            make_shared<MockAsyncExecutor>(), gcp_database_factory_) {
    instance_client_->instance_resource_name = kInstanceResourceName;

    GetInstanceByNameRequest get_request;
    get_request.set_instance_name(kInstanceName);
    get_instance_context_.request =
        make_shared<GetInstanceByNameRequest>(move(get_request));
    get_instance_context_.callback = [this](auto) { finish_called_ = true; };

    ListInstancesByStatusRequest list_request;
    list_request.set_instance_status(InstanceStatus::TERMINATING_WAIT);
    list_instances_context_.request =
        make_shared<ListInstancesByStatusRequest>(move(list_request));
    list_instances_context_.callback = [this](auto) { finish_called_ = true; };

    ON_CALL(*connection_, Commit).WillByDefault(Return(CommitResult{}));
    ON_CALL(*gcp_database_factory_, CreateClient)
        .WillByDefault(Return(make_shared<Client>(connection_)));

    instance_1_.set_instance_name(kInstanceName);
    instance_1_.set_status(InstanceStatus::TERMINATING_WAIT);
    *instance_1_.mutable_request_time() = request_time_1_;
    *instance_1_.mutable_termination_time() = termination_time_1_;
    instance_1_.set_ttl(ttl_1_.seconds());

    instance_2_.set_instance_name(kInstanceName);
    instance_2_.set_status(InstanceStatus::TERMINATING_WAIT);
    *instance_2_.mutable_request_time() = request_time_2_;
    *instance_2_.mutable_termination_time() = termination_time_2_;
    instance_2_.set_ttl(ttl_2_.seconds());

    update_instance_context_.request = make_shared<UpdateInstanceRequest>();
    *update_instance_context_.request->mutable_instance() = instance_1_;
    update_instance_mutation_ =
        MakeUpdateMutation(kInstanceTableName,
                           {kInstanceNameColumnName, kInstanceStatusColumnName,
                            kRequestTimeColumnName, kTerminationTimeColumnName},
                           kInstanceName, "TERMINATING_WAIT",
                           MakeTimestamp(request_time_1_).value(),
                           MakeTimestamp(termination_time_1_).value());

    EXPECT_SUCCESS(instance_database_client_.Init());
    EXPECT_SUCCESS(instance_database_client_.Run());
  }

  shared_ptr<MockInstanceClientProvider> instance_client_;
  shared_ptr<MockConnection> connection_;
  shared_ptr<MockGcpDatabaseFactory> gcp_database_factory_;
  GcpInstanceDatabaseClientProvider instance_database_client_;

  AsyncContext<GetInstanceByNameRequest, GetInstanceByNameResponse>
      get_instance_context_;
  AsyncContext<ListInstancesByStatusRequest, ListInstancesByStatusResponse>
      list_instances_context_;
  AsyncContext<UpdateInstanceRequest, UpdateInstanceResponse>
      update_instance_context_;

  protobuf::Timestamp request_time_1_ = MakeProtoTimestamp(100000, 1000);
  protobuf::Timestamp termination_time_1_ = MakeProtoTimestamp(200000, 2000);
  protobuf::Timestamp ttl_1_ = MakeProtoTimestamp(300000, 3000);
  AsgInstance instance_1_;
  protobuf::Timestamp request_time_2_ = MakeProtoTimestamp(400000, 4000);
  protobuf::Timestamp termination_time_2_ = MakeProtoTimestamp(500000, 5000);
  protobuf::Timestamp ttl_2_ = MakeProtoTimestamp(600000, 6000);
  AsgInstance instance_2_;

  Mutation update_instance_mutation_;

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  std::atomic_bool finish_called_{false};
};

TEST_F(GcpInstanceDatabaseClientProviderTests, InitWithGetProjectIdFailure) {
  auto instance_client = make_shared<NiceMock<MockInstanceClientProvider>>();
  instance_client->get_instance_resource_name_mock =
      FailureExecutionResult(123);
  GcpInstanceDatabaseClientProvider instance_database_client(
      make_shared<InstanceDatabaseClientOptions>(
          kSpannerInstanceName, kSpannerDatabaseName, kInstanceTableName),
      instance_client, make_shared<MockAsyncExecutor>(),
      make_shared<MockAsyncExecutor>(), gcp_database_factory_);

  EXPECT_SUCCESS(instance_database_client.Init());
  EXPECT_THAT(instance_database_client.Run(),
              ResultIs(FailureExecutionResult(123)));
}

MATCHER_P(SqlEqual, expected_sql, "") {
  string no_whitespace_arg_sql = arg.statement.sql();
  absl::RemoveExtraAsciiWhitespace(&no_whitespace_arg_sql);
  string no_whitespace_expected_sql = expected_sql.sql();
  absl::RemoveExtraAsciiWhitespace(&no_whitespace_expected_sql);

  SqlStatement modified_arg(no_whitespace_arg_sql, arg.statement.params());
  SqlStatement modified_expected(no_whitespace_expected_sql,
                                 expected_sql.params());

  if (!ExplainMatchResult(Eq(modified_expected), modified_arg,
                          result_listener)) {
    string actual =
        absl::StrFormat("Actual - SQL: \"%s\"", no_whitespace_arg_sql);
    for (const auto& [name, val] : modified_arg.params()) {
      absl::StrAppend(&actual, absl::StrFormat("\n[param]: {%s=%s}", name,
                                               PrintToString(val)));
    }
    *result_listener << actual;
    return false;
  }
  return true;
}

TEST_F(GcpInstanceDatabaseClientProviderTests, GetInstanceByNameSucceeded) {
  auto returned_row = MakeRow(kInstanceName, "TERMINATING_WAIT",
                              MakeTimestamp(request_time_1_).value(),
                              MakeTimestamp(termination_time_1_).value(),
                              MakeTimestamp(ttl_1_).value());
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_,
              ExecuteQuery(SqlEqual(SqlStatement(kExpectGetInstanceQuery))))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  get_instance_context_.callback = [&](auto& context) {
    EXPECT_SUCCESS(context.result);

    const auto& response = context.response;
    ASSERT_THAT(response, NotNull());

    GetInstanceByNameResponse expected_response;
    *expected_response.mutable_instance() = instance_1_;
    EXPECT_THAT(*response, EqualsProto(expected_response));

    finish_called_ = true;
  };

  instance_database_client_.GetInstanceByName(get_instance_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpInstanceDatabaseClientProviderTests, GetInstanceByNameRowNotFound) {
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow).WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_,
              ExecuteQuery(SqlEqual(SqlStatement(kExpectGetInstanceQuery))))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  get_instance_context_.callback = [&](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND)));

    finish_called_ = true;
  };

  instance_database_client_.GetInstanceByName(get_instance_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpInstanceDatabaseClientProviderTests, GetInstanceByNameRowParseError) {
  auto returned_row = MakeRow(
      kInstanceName, "TERMINATING_WAIT", MakeTimestamp(request_time_1_).value(),
      MakeTimestamp(termination_time_1_).value(), 300000);
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row))
      .WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_,
              ExecuteQuery(SqlEqual(SqlStatement(kExpectGetInstanceQuery))))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  get_instance_context_.callback = [&](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));

    finish_called_ = true;
  };

  instance_database_client_.GetInstanceByName(get_instance_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpInstanceDatabaseClientProviderTests, ListInstancesByStatusSucceeded) {
  auto returned_row_1 = MakeRow(kInstanceName, "TERMINATING_WAIT",
                                MakeTimestamp(request_time_1_).value(),
                                MakeTimestamp(termination_time_1_).value(),
                                MakeTimestamp(ttl_1_).value());

  auto returned_row_2 = MakeRow(kInstanceName, "TERMINATING_WAIT",
                                MakeTimestamp(request_time_2_).value(),
                                MakeTimestamp(termination_time_2_).value(),
                                MakeTimestamp(ttl_2_).value());
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row_1))
      .WillOnce(Return(returned_row_2))
      .WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_,
              ExecuteQuery(SqlEqual(SqlStatement(kExpectListInstancesQuery))))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  list_instances_context_.callback = [&](auto& context) {
    EXPECT_SUCCESS(context.result);

    const auto& response = context.response;
    ASSERT_THAT(response, NotNull());

    ListInstancesByStatusResponse expected_response;
    *expected_response.add_instances() = instance_1_;
    *expected_response.add_instances() = instance_2_;
    EXPECT_THAT(*response, EqualsProto(expected_response));

    finish_called_ = true;
  };

  instance_database_client_.ListInstancesByStatus(list_instances_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpInstanceDatabaseClientProviderTests,
       ListInstancesByStatusRowNotFound) {
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow).WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_,
              ExecuteQuery(SqlEqual(SqlStatement(kExpectListInstancesQuery))))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  list_instances_context_.callback = [&](auto& context) {
    EXPECT_SUCCESS(context.result);

    const auto& response = context.response;
    ASSERT_THAT(response, NotNull());

    ListInstancesByStatusResponse expected_response;
    EXPECT_THAT(*response, EqualsProto(expected_response));

    finish_called_ = true;
  };

  instance_database_client_.ListInstancesByStatus(list_instances_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpInstanceDatabaseClientProviderTests,
       ListInstancesByStatusInstanceParseError) {
  auto returned_row_1 = MakeRow(kInstanceName, "TERMINATING_WAIT",
                                MakeTimestamp(request_time_1_).value(),
                                MakeTimestamp(termination_time_1_).value(),
                                MakeTimestamp(ttl_1_).value());
  auto returned_row_2 =
      MakeRow(kInstanceName, "INVALID", MakeTimestamp(request_time_2_).value(),
              MakeTimestamp(termination_time_2_).value(),
              MakeTimestamp(ttl_2_).value());
  auto returned_results = make_unique<MockResultSetSource>();
  EXPECT_CALL(*returned_results, NextRow)
      .WillOnce(Return(returned_row_1))
      .WillOnce(Return(returned_row_2))
      .WillRepeatedly(Return(Row()));

  EXPECT_CALL(*connection_,
              ExecuteQuery(SqlEqual(SqlStatement(kExpectListInstancesQuery))))
      .WillOnce(Return(ByMove(RowStream(std::move(returned_results)))));

  list_instances_context_.callback = [&](auto& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(
            SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INVALID_INSTANCE_STATUS)));

    finish_called_ = true;
  };

  instance_database_client_.ListInstancesByStatus(list_instances_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpInstanceDatabaseClientProviderTests, UpdateInstanceSucceeded) {
  EXPECT_CALL(
      *connection_,
      Commit(FieldsAre(_, UnorderedElementsAre(update_instance_mutation_), _)))
      .WillOnce(Return(CommitResult{}));

  update_instance_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  instance_database_client_.UpdateInstance(update_instance_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpInstanceDatabaseClientProviderTests,
       UpdateInstanceWithCommitFailure) {
  EXPECT_CALL(
      *connection_,
      Commit(FieldsAre(_, UnorderedElementsAre(update_instance_mutation_), _)))
      .WillOnce(Return(Status(google::cloud::StatusCode::kInternal, "Error")));

  update_instance_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(RetryExecutionResult(
                    SCP_INSTANCE_DATABASE_CLIENT_PROVIDER_COMMIT_FAILED)));

    finish_called_ = true;
  };

  instance_database_client_.UpdateInstance(update_instance_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpInstanceDatabaseClientProviderTests,
       UpdateInstanceWithRecordNotFoundFailure) {
  EXPECT_CALL(
      *connection_,
      Commit(FieldsAre(_, UnorderedElementsAre(update_instance_mutation_), _)))
      .WillOnce(Return(Status(google::cloud::StatusCode::kNotFound, "Error")));

  update_instance_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND)));

    finish_called_ = true;
  };

  instance_database_client_.UpdateInstance(update_instance_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
