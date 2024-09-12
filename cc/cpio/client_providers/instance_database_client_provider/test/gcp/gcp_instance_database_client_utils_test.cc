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

#include "cpio/client_providers/instance_database_client_provider/src/gcp/gcp_instance_database_client_utils.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/timestamp_test_utils.h"
#include "cpio/client_providers/instance_database_client_provider/src/common/error_codes.h"
#include "cpio/common/src/gcp/error_codes.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/cloud/spanner/value.h"
#include "operator/protos/shared/backend/asginstance/asg_instance.pb.h"
#include "operator/protos/shared/backend/asginstance/instance_status.pb.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::cloud::spanner_mocks::MakeRow;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_GCP_UNKNOWN;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INVALID_INSTANCE_STATUS;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_UNEXPECTED_COLUMN_COUNT;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::MakeProtoTimestamp;
using google::scp::core::test::ResultIs;
using google::scp::operators::protos::shared::backend::asginstance::AsgInstance;
using google::scp::operators::protos::shared::backend::asginstance::
    InstanceStatus;
using std::get;
using std::shared_ptr;
using std::string;
using std::vector;
using testing::Eq;
using testing::ExplainMatchResult;

namespace {
constexpr char kInstanceName[] =
    "https://compute.googleapis.com/projects/123456789/zones/us-central1-c/"
    "instances/987654321";
}  // namespace

namespace google::scp::cpio::client_providers::test {

class ConvertJsonToInstanceTests : public testing::Test {
 protected:
  ConvertJsonToInstanceTests() {
    expected_instance_1_.set_instance_name(kInstanceName);
    expected_instance_1_.set_status(InstanceStatus::TERMINATING_WAIT);
    *expected_instance_1_.mutable_request_time() = request_time_1_;
    *expected_instance_1_.mutable_termination_time() = termination_time_1_;
    expected_instance_1_.set_ttl(ttl_1_.seconds());

    expected_instance_2_.set_instance_name(kInstanceName);
    expected_instance_2_.set_status(InstanceStatus::TERMINATING_WAIT);
    *expected_instance_2_.mutable_request_time() = request_time_2_;
    *expected_instance_2_.mutable_termination_time() = termination_time_2_;
    expected_instance_2_.set_ttl(ttl_2_.seconds());
  }

  protobuf::Timestamp request_time_1_ = MakeProtoTimestamp(100000, 1000);
  protobuf::Timestamp termination_time_1_ = MakeProtoTimestamp(200000, 2000);
  protobuf::Timestamp ttl_1_ = MakeProtoTimestamp(300000, 3000);
  AsgInstance expected_instance_1_;
  protobuf::Timestamp request_time_2_ = MakeProtoTimestamp(400000, 4000);
  protobuf::Timestamp termination_time_2_ = MakeProtoTimestamp(500000, 5000);
  protobuf::Timestamp ttl_2_ = MakeProtoTimestamp(600000, 6000);
  AsgInstance expected_instance_2_;
};

TEST_F(ConvertJsonToInstanceTests, Success) {
  auto returned_row =
      MakeRow(kInstanceName, "TERMINATING_WAIT",
              cloud::spanner::MakeTimestamp(request_time_1_).value(),
              cloud::spanner::MakeTimestamp(termination_time_1_).value(),
              cloud::spanner::MakeTimestamp(ttl_1_).value());

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      IsSuccessfulAndHolds(EqualsProto(expected_instance_1_)));
}

TEST_F(ConvertJsonToInstanceTests, FailureWithInvalidInstanceName) {
  auto returned_row =
      MakeRow(100, "TERMINATING_WAIT",
              cloud::spanner::MakeTimestamp(request_time_1_).value(),
              cloud::spanner::MakeTimestamp(termination_time_1_).value(),
              cloud::spanner::MakeTimestamp(ttl_1_).value());

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));
}

TEST_F(ConvertJsonToInstanceTests, FailureWithInvalidInstanceStatusType) {
  auto returned_row =
      MakeRow(kInstanceName, 100,
              cloud::spanner::MakeTimestamp(request_time_1_).value(),
              cloud::spanner::MakeTimestamp(termination_time_1_).value(),
              cloud::spanner::MakeTimestamp(ttl_1_).value());

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));
}

TEST_F(ConvertJsonToInstanceTests, FailureWithInvalidInstanceStatus) {
  auto returned_row =
      MakeRow(kInstanceName, "INVALID",
              cloud::spanner::MakeTimestamp(request_time_1_).value(),
              cloud::spanner::MakeTimestamp(termination_time_1_).value(),
              cloud::spanner::MakeTimestamp(ttl_1_).value());

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      ResultIs(FailureExecutionResult(
          SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INVALID_INSTANCE_STATUS)));
}

TEST_F(ConvertJsonToInstanceTests, FailureWithInvalidRequestTime) {
  auto returned_row =
      MakeRow(kInstanceName, "TERMINATING_WAIT", 1000,
              cloud::spanner::MakeTimestamp(termination_time_1_).value(),
              cloud::spanner::MakeTimestamp(ttl_1_).value());

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));
}

TEST_F(ConvertJsonToInstanceTests, FailureWithInvalidTerminationTime) {
  auto returned_row =
      MakeRow(kInstanceName, "TERMINATING_WAIT",
              cloud::spanner::MakeTimestamp(request_time_1_).value(), 1000,
              cloud::spanner::MakeTimestamp(ttl_1_).value());

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));
}

TEST_F(ConvertJsonToInstanceTests, FailureWithInvalidTtl) {
  auto returned_row =
      MakeRow(kInstanceName, "TERMINATING_WAIT",
              cloud::spanner::MakeTimestamp(request_time_1_).value(),
              cloud::spanner::MakeTimestamp(termination_time_1_).value(), 100);

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));
}

TEST_F(ConvertJsonToInstanceTests, FailureWithColumnSizeTooSmall) {
  auto returned_row =
      MakeRow(kInstanceName, "TERMINATING_WAIT",
              cloud::spanner::MakeTimestamp(request_time_1_).value(),
              cloud::spanner::MakeTimestamp(termination_time_1_).value());

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      ResultIs(FailureExecutionResult(
          SC_INSTANCE_DATABASE_CLIENT_PROVIDER_UNEXPECTED_COLUMN_COUNT)));
}

TEST_F(ConvertJsonToInstanceTests, FailureWithColumnSizeTooBig) {
  auto returned_row =
      MakeRow(kInstanceName, "TERMINATING_WAIT",
              cloud::spanner::MakeTimestamp(request_time_1_).value(),
              cloud::spanner::MakeTimestamp(termination_time_1_).value(),
              cloud::spanner::MakeTimestamp(ttl_1_).value(), 100);

  EXPECT_THAT(
      GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(returned_row),
      ResultIs(FailureExecutionResult(
          SC_INSTANCE_DATABASE_CLIENT_PROVIDER_UNEXPECTED_COLUMN_COUNT)));
}
}  // namespace google::scp::cpio::client_providers::test
