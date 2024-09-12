
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

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <utility>

#include "cpio/client_providers/common/src/gcp/gcp_database_factory.h"

namespace google::scp::cpio::client_providers::mock {

class MockGcpDatabaseFactory : public GcpDatabaseFactory {
 public:
  explicit MockGcpDatabaseFactory(
      std::shared_ptr<DatabaseClientOptions> options)
      : GcpDatabaseFactory(options) {}

  MOCK_METHOD(
      (core::ExecutionResultOr<std::pair<
           std::shared_ptr<cloud::spanner::Client>,
           std::shared_ptr<cloud::spanner_admin::DatabaseAdminClient>>>),
      CreateClients, (const std::string&), (noexcept, override));
  MOCK_METHOD(
      (core::ExecutionResultOr<std::shared_ptr<cloud::spanner::Client>>),
      CreateClient, (const std::string&), (noexcept, override));
  MOCK_METHOD((cloud::Options), CreateClientOptions, (), (noexcept, override));
};
}  // namespace google::scp::cpio::client_providers::mock
