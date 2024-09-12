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

#pragma once

#include <memory>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

namespace google::scp::cpio::client_providers {
constexpr char kJobIdInMessageBodyKeyName[] = "jobRequestId";
constexpr char kJobsTablePartitionKeyName[] = "JobId";
constexpr char kServerJobIdColumnName[] = "serverJobId";
constexpr char kJobBodyColumnName[] = "jobBody";
constexpr char kJobStatusColumnName[] = "jobStatus";
constexpr char kCreatedTimeColumnName[] = "createdTime";
constexpr char kUpdatedTimeColumnName[] = "updatedTime";
constexpr char kRetryCountColumnName[] = "retryCount";
constexpr char kProcessingStartedTimeColumnName[] = "processingStartedTime";

struct JobMessageBody {
  std::string job_id;
  std::string server_job_id;

  JobMessageBody(const std::string& job_id, const std::string& server_job_id)
      : job_id(job_id), server_job_id(server_job_id) {}

  /**
   * @brief Create a JobMessageBody with Job ID and Server Job ID.
   *
   * @param job_id Job ID.
   * @param server_job_id Server Job ID.
   * @return string The message contains Job ID and Server Job ID in JSON
   * format.
   */
  explicit JobMessageBody(const std::string& json_string) {
    auto message_body = nlohmann::json::parse(json_string);
    job_id = message_body[kJobIdInMessageBodyKeyName];
    server_job_id = message_body[kServerJobIdColumnName];
  }

  /**
   * @brief Convert a Json string into Job ID and Server Job ID.
   *
   * @param json_string The Json string contains Job ID and Server Job ID.
   * @return pair<string, string> The pair of Job ID and Server Job ID.
   */
  std::string ToJsonString() {
    nlohmann::json message_body;
    message_body[kJobIdInMessageBodyKeyName] = job_id;
    message_body[kServerJobIdColumnName] = server_job_id;
    return message_body.dump();
  }
};

class JobClientUtils {
 public:
  /**
   * @brief Make a string item attribute from name and value.
   *
   * @param name The name of the item attribute.
   * @param value The value of the item attribute.
   * @return google::cmrt::sdk::nosql_database_service::v1::ItemAttribute The
   * created item attribute.
   */
  static google::cmrt::sdk::nosql_database_service::v1::ItemAttribute
  MakeStringAttribute(const std::string& name,
                      const std::string& value) noexcept;

  /**
   * @brief Make a json string item attribute from name and value.
   *
   * @param name The name of the item attribute.
   * @param value The value of the item attribute.
   * @return google::cmrt::sdk::nosql_database_service::v1::ItemAttribute The
   * created item attribute.
   */
  static google::cmrt::sdk::nosql_database_service::v1::ItemAttribute
  MakeJsonStringAttribute(const std::string& name,
                          const std::string& value) noexcept;

  /**
   * @brief Make an int item attribute from name and value.
   *
   * @param name The name of the item attribute.
   * @param value The value of the item attribute.
   * @return google::cmrt::sdk::nosql_database_service::v1::ItemAttribute The
   * created item attribute.
   */
  static google::cmrt::sdk::nosql_database_service::v1::ItemAttribute
  MakeIntAttribute(const std::string& name, const int32_t value) noexcept;

  /**
   * @brief Create a job item.
   *
   * @param job_id Job ID.
   * @param server_job_id Server job ID.
   * @param job_body Job Body.
   * @param job_status The status of the job.
   * @param created_time The created time of the job.
   * @param updated_time The updated time of the job.
   * @param processing_started_time The started time of the job.
   * @param retry_count The number of times the job has been attempted for
   * processing.
   * @return google::cmrt::sdk::job_service::v1::Job The created job.
   */
  static google::cmrt::sdk::job_service::v1::Job CreateJob(
      const std::string& job_id, const std::string& server_job_id,
      const std::string& job_body,
      const google::cmrt::sdk::job_service::v1::JobStatus& job_status,
      const google::protobuf::Timestamp& created_time,
      const google::protobuf::Timestamp& updated_time,
      const google::protobuf::Timestamp& processing_started_time,
      const int retry_count = 0) noexcept;

  /**
   * @brief Convert google::cmrt::sdk::nosql_database_service::v1::Item to
   * google::cmrt::sdk::job_service::v1::Job.
   *
   * @param item The item from NoSQL database.
   * @return
   * google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest
   * The request for job reception.
   */
  static core::ExecutionResultOr<google::cmrt::sdk::job_service::v1::Job>
  ConvertDatabaseItemToJob(
      const google::cmrt::sdk::nosql_database_service::v1::Item& item) noexcept;

  /**
   * @brief Create an CreateDatabaseItemRequest for job creation.
   *
   * @param job_table_name The name of the table to create.
   * @param job Job.
   * @param ttl Ttl of the job.
   * @return
   * google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest
   * The request for created job in the database.
   */
  static core::ExecutionResultOr<
      google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest>
  CreatePutJobRequest(
      const std::string& job_table_name,
      const google::cmrt::sdk::job_service::v1::Job& job,
      const std::optional<google::protobuf::Duration>& ttl) noexcept;

  /**
   * @brief Create an UpsertDatabaseItemRequest for job update. The signature
   * has all parameters for upsert request, but only job_table_name and job_id
   * in the job are required. Parameters and the fields in the job that are not
   * set and are in default values will not be added to the attributes of the
   * request.
   *
   *
   * @param job_table_name The name of the table to upsert.
   * @param job Job.
   * @return
   * google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest
   * The request for created job upsertion.
   */
  static core::ExecutionResultOr<
      google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest>
  CreateUpsertJobRequest(
      const std::string& job_table_name,
      const google::cmrt::sdk::job_service::v1::Job& job) noexcept;

  /**
   * @brief Create an GetDatabaseItemRequest for get next job from database.
   *
   *
   * server_job_id is a requried attribute in the GetDatabaseItemRequest,
   * because this field is always unique for the job. This request will only be
   * succeed if the job entry in the table has the same server_job_id in the
   * job message in the queue.
   *
   * @param job_table_name The name of the table to upsert.
   * @param job_id The job id of the job to get.
   * @param server_job_id The server job id of the job to get.
   * @return
   * google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest
   * The request for get job from database.
   */
  static std::shared_ptr<
      google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest>
  CreateGetNextJobRequest(const std::string& job_table_name,
                          const std::string& job_id,
                          const std::string& server_job_id) noexcept;

  /**
   * @brief Create an GetDatabaseItemRequest for get job by job id from
   * database.
   *
   * @param job_table_name The name of the table to upsert.
   * @param job_id The job id of the job to get.
   * @return
   * google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest
   * The request for get job by job id from database.
   */
  static std::shared_ptr<
      google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest>
  CreateGetJobByJobIdRequest(const std::string& job_table_name,
                             const std::string& job_id) noexcept;

  /**
   * @brief Validate job status.
   *
   * @param current_status The status of current job.
   * @param update_status The status is going to update.
   * @return core::ExecutionResult The validation result.
   */
  static core::ExecutionResult ValidateJobStatus(
      const google::cmrt::sdk::job_service::v1::JobStatus& current_status,
      const google::cmrt::sdk::job_service::v1::JobStatus&
          update_status) noexcept;
};
}  // namespace google::scp::cpio::client_providers
