/*
 * Copyright 2023 Google LLC
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

#include "google/cloud/options.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mutations.h"
#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio::client_providers {
/// Creates GCP cloud::spanner::Client.
class GcpDatabaseFactory {
 public:
  explicit GcpDatabaseFactory(std::shared_ptr<DatabaseClientOptions> options)
      : options_(options) {}

  virtual cloud::Options CreateClientOptions() noexcept;

  virtual core::ExecutionResultOr<std::pair<
      std::shared_ptr<google::cloud::spanner::Client>,
      std::shared_ptr<google::cloud::spanner_admin::DatabaseAdminClient>>>
  CreateClients(const std::string& project_id) noexcept;

  virtual core::ExecutionResultOr<
      std::shared_ptr<google::cloud::spanner::Client>>
  CreateClient(const std::string& project_id) noexcept;

  virtual ~GcpDatabaseFactory() = default;

 protected:
  /// Options for the factory.
  std::shared_ptr<DatabaseClientOptions> options_;
};

}  // namespace google::scp::cpio::client_providers
