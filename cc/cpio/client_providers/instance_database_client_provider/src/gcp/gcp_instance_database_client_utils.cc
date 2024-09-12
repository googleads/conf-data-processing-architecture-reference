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

#include "gcp_instance_database_client_utils.h"

#include <string>
#include <utility>
#include <variant>

#include <nlohmann/json.hpp>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "cpio/client_providers/instance_database_client_provider/src/common/error_codes.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/value.h"
#include "google/protobuf/util/time_util.h"
#include "operator/protos/shared/backend/asginstance/asg_instance.pb.h"
#include "operator/protos/shared/backend/asginstance/instance_status.pb.h"
#include "public/core/interface/execution_result.h"

using google::cloud::spanner::Row;
using google::cloud::spanner::Timestamp;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INVALID_INSTANCE_STATUS;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_UNEXPECTED_COLUMN_COUNT;
using google::scp::cpio::common::GcpUtils;
using google::scp::operators::protos::shared::backend::asginstance::AsgInstance;
using google::scp::operators::protos::shared::backend::asginstance::
    InstanceStatus;
using google::scp::operators::protos::shared::backend::asginstance::
    InstanceStatus_Parse;
using std::move;
using std::string;

namespace {
constexpr char kGcpInstanceDatabaseClientUtils[] =
    "GcpInstanceDatabaseClientUtils";
constexpr int8_t kInstanceTableColumnCount = 5;
}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResultOr<protobuf::Timestamp> GetTimestampFromRow(
    const Row& row, const int8_t row_index) {
  const auto timestamp_or = row.values()[row_index].get<Timestamp>();
  if (!timestamp_or.ok()) {
    RETURN_AND_LOG_IF_FAILURE(
        GcpUtils::GcpErrorConverter(timestamp_or.status()),
        kGcpInstanceDatabaseClientUtils, kZeroUuid,
        "Spanner get timestamp failed");
  }
  auto timestamp_proto_or = timestamp_or->get<protobuf::Timestamp>();
  if (!timestamp_proto_or.ok()) {
    RETURN_AND_LOG_IF_FAILURE(
        GcpUtils::GcpErrorConverter(timestamp_proto_or.status()),
        kGcpInstanceDatabaseClientUtils, kZeroUuid,
        "Convert to Proto Timestamp failed");
  }
  return *timestamp_proto_or;
}

ExecutionResultOr<AsgInstance>
GcpInstanceDatabaseClientUtils::ConvertJsonToInstance(const Row& row) noexcept {
  if (row.size() != kInstanceTableColumnCount) {
    return FailureExecutionResult(
        SC_INSTANCE_DATABASE_CLIENT_PROVIDER_UNEXPECTED_COLUMN_COUNT);
  }

  // The column order is determined in the SQL query:
  // SELECT kInstanceNameColumnName, kInstanceStatusColumnName,
  //        kRequestTimeColumnName, kTerminationTimeColumnName,
  //        kTTLColumnName
  AsgInstance instance;
  const auto instance_name_or = row.values()[0].get<string>();
  if (!instance_name_or.ok()) {
    RETURN_AND_LOG_IF_FAILURE(
        GcpUtils::GcpErrorConverter(instance_name_or.status()),
        kGcpInstanceDatabaseClientUtils, kZeroUuid,
        "Spanner get instance name failed");
  }
  instance.set_instance_name(move(*instance_name_or));

  const auto instance_status_or = row.values()[1].get<string>();
  if (!instance_status_or.ok()) {
    RETURN_AND_LOG_IF_FAILURE(
        GcpUtils::GcpErrorConverter(instance_status_or.status()),
        kGcpInstanceDatabaseClientUtils, kZeroUuid,
        "Spanner get instance status failed");
  }
  InstanceStatus instance_status;
  if (!InstanceStatus_Parse(*instance_status_or, &instance_status)) {
    RETURN_AND_LOG_IF_FAILURE(
        FailureExecutionResult(
            SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INVALID_INSTANCE_STATUS),
        kGcpInstanceDatabaseClientUtils, kZeroUuid, "Invalid instance status");
  }
  instance.set_status(instance_status);

  auto request_time_or = GetTimestampFromRow(row, 2);
  if (!request_time_or.Successful()) {
    SCP_ERROR(kGcpInstanceDatabaseClientUtils, kZeroUuid,
              request_time_or.result(), "Spanner get request time failed");
    return request_time_or.result();
  }
  *instance.mutable_request_time() = move(*request_time_or);

  auto termination_time_or = GetTimestampFromRow(row, 3);
  if (!termination_time_or.Successful()) {
    SCP_ERROR(kGcpInstanceDatabaseClientUtils, kZeroUuid,
              termination_time_or.result(),
              "Spanner get termination time failed");
    return termination_time_or.result();
  }
  *instance.mutable_termination_time() = move(*termination_time_or);

  auto ttl_or = GetTimestampFromRow(row, 4);
  if (!ttl_or.Successful()) {
    SCP_ERROR(kGcpInstanceDatabaseClientUtils, kZeroUuid, ttl_or.result(),
              "Spanner get TTL failed");
    return ttl_or.result();
  }
  instance.set_ttl(ttl_or->seconds());

  return instance;
}

}  // namespace google::scp::cpio::client_providers
