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

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/error_codes.h"

namespace google::scp::core::errors {
/// Registers component code as 0x0204 for GCP metric client.
REGISTER_COMPONENT_CODE(SC_GCP_METRIC_CLIENT, 0x0204)

DEFINE_ERROR_CODE(
    SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP, SC_GCP_METRIC_CLIENT,
    0x0001,
    "Valid timestamp must not be "
    "more than 25 hours in the past or more than five minutes in the future.",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_GCP_METRIC_CLIENT_FAILED_OVERSIZE_METRIC_LABELS, SC_GCP_METRIC_CLIENT,
    0x0002, "Gcp custom metric labels must not have a size greater than 30",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_GCP_METRIC_CLIENT_INVALID_METRIC_VALUE,
                  SC_GCP_METRIC_CLIENT, 0x0003,
                  "Failed to parse Gcp custom metric value to double",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_GCP_METRIC_CLIENT_INVALID_METRIC_LABEL_KEY, SC_GCP_METRIC_CLIENT, 0x0004,
    "GCP custom metric label keys must be less than 100 characters long",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_GCP_METRIC_CLIENT_INVALID_METRIC_LABEL_VALUE, SC_GCP_METRIC_CLIENT,
    0x0005,
    "GCP custom metric label values must be less than 1024 characters long",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_GCP_METRIC_CLIENT_TOO_MANY_METRICS_IN_ONE_REQUEST,
                  SC_GCP_METRIC_CLIENT, 0x0006,
                  "A single GCP custom metric create request at most could "
                  "have 200 metrics (time series), and PutMetricsRequest "
                  "contains too many metrics",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_GCP_METRIC_CLIENT_DUPLICATE_METRIC_IN_ONE_REQUEST,
                  SC_GCP_METRIC_CLIENT, 0x0007,
                  "Only one data point per metric per request",
                  HttpStatusCode::BAD_REQUEST)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP,
                         SC_CPIO_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_METRIC_CLIENT_FAILED_OVERSIZE_METRIC_LABELS,
                         SC_CPIO_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_METRIC_CLIENT_INVALID_METRIC_VALUE,
                         SC_CPIO_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_METRIC_CLIENT_INVALID_METRIC_LABEL_KEY,
                         SC_CPIO_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_METRIC_CLIENT_INVALID_METRIC_LABEL_VALUE,
                         SC_CPIO_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_METRIC_CLIENT_TOO_MANY_METRICS_IN_ONE_REQUEST,
                         SC_CPIO_INVALID_ARGUMENT)
MAP_TO_PUBLIC_ERROR_CODE(SC_GCP_METRIC_CLIENT_DUPLICATE_METRIC_IN_ONE_REQUEST,
                         SC_CPIO_INVALID_ARGUMENT)
}  // namespace google::scp::core::errors
