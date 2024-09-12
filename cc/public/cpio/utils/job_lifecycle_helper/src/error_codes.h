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

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
REGISTER_COMPONENT_CODE(SC_JOB_LIFECYCLE_HELPER, 0x021E)

DEFINE_ERROR_CODE(SC_JOB_LIFECYCLE_HELPER_MISSING_JOB_ID,
                  SC_JOB_LIFECYCLE_HELPER, 0x0001, "Job id is not found",
                  HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(
    SC_JOB_LIFECYCLE_HELPER_ORPHANED_JOB_FOUND, SC_JOB_LIFECYCLE_HELPER, 0x0002,
    "Job preparation failed due to the next job message in the queue does "
    "not have corresponding job entry in the database",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOB_LIFECYCLE_HELPER_JOB_ALREADY_COMPLETED,
                  SC_JOB_LIFECYCLE_HELPER, 0x0003,
                  "Job preparation failed due to the next job message in the "
                  "queue is already completed",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_JOB_LIFECYCLE_HELPER_RETRY_EXHAUSTED, SC_JOB_LIFECYCLE_HELPER, 0x0004,
    "Job preparation failed due to the retry count of the job is exceeding "
    "retry limit.",
    HttpStatusCode::TOO_MANY_REQUESTS)

DEFINE_ERROR_CODE(SC_JOB_LIFECYCLE_HELPER_INVALID_JOB_STATUS,
                  SC_JOB_LIFECYCLE_HELPER, 0x0005, "Invalid job status.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_JOB_LIFECYCLE_HELPER_MISSING_RECEIPT_INFO, SC_JOB_LIFECYCLE_HELPER,
    0x0006,
    "Receipt info for current job id is missing in the job metadata map.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOB_LIFECYCLE_HELPER_CURRENT_INSTANCE_IS_TERMINATING,
                  SC_JOB_LIFECYCLE_HELPER, 0x0007,
                  "Job preparation failed due to current instance is scheduled "
                  "for termination.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOB_LIFECYCLE_HELPER_MISSING_METRIC_INSTANCE_FACTORY,
                  SC_JOB_LIFECYCLE_HELPER, 0x0008,
                  "Metric instance factory is not found.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(
    SC_JOB_LIFECYCLE_HELPER_JOB_BEING_PROCESSING, SC_JOB_LIFECYCLE_HELPER,
    0x0009,
    "Job preparation failed due next job is processing by another worker.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_JOB_LIFECYCLE_HELPER_INVALID_DURATION_BEFORE_RELEASE,
                  SC_JOB_LIFECYCLE_HELPER, 0x000A,
                  "Job release failed due to invalid duration before release.",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
