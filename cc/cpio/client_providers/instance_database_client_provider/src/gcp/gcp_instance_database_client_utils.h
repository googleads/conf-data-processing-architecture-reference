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

#include <string>
#include <variant>

#include <nlohmann/json.hpp>

#include "cpio/client_providers/instance_database_client_provider/src/common/error_codes.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/value.h"
#include "operator/protos/shared/backend/asginstance/asg_instance.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Provides utility functions for GCP Spanner request flows. GCP uses
 * custom types that need to be converted to SCP types during runtime.
 */
class GcpInstanceDatabaseClientUtils {
 public:
  static core::ExecutionResultOr<
      scp::operators::protos::shared::backend::asginstance ::AsgInstance>
  ConvertJsonToInstance(const cloud::spanner::Row& row) noexcept;
};
}  // namespace google::scp::cpio::client_providers
