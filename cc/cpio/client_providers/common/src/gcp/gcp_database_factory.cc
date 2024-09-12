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

#include "gcp_database_factory.h"

#include <memory>
#include <string>

#include "google/cloud/options.h"

using google::cloud::Options;
using google::cloud::spanner::Client;
using google::cloud::spanner::Database;
using google::cloud::spanner::MakeConnection;
using google::cloud::spanner_admin::DatabaseAdminClient;
using google::cloud::spanner_admin::MakeDatabaseAdminConnection;
using google::scp::core::ExecutionResultOr;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
ExecutionResultOr<pair<shared_ptr<Client>, shared_ptr<DatabaseAdminClient>>>
GcpDatabaseFactory::CreateClients(const string& project_id) noexcept {
  auto client_options = CreateClientOptions();
  return make_pair(make_shared<Client>(MakeConnection(
                       Database(project_id, options_->gcp_spanner_instance_name,
                                options_->gcp_spanner_database_name),
                       client_options)),
                   make_shared<DatabaseAdminClient>(
                       MakeDatabaseAdminConnection(client_options)));
}

ExecutionResultOr<shared_ptr<Client>> GcpDatabaseFactory::CreateClient(
    const string& project_id) noexcept {
  auto client_options = CreateClientOptions();
  return make_shared<Client>(
      MakeConnection(Database(project_id, options_->gcp_spanner_instance_name,
                              options_->gcp_spanner_database_name),
                     client_options));
}

Options GcpDatabaseFactory::CreateClientOptions() noexcept {
  return Options();
}
}  // namespace google::scp::cpio::client_providers
