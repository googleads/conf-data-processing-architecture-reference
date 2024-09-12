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

#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/nosql_database_client/src/nosql_database_client.h"
#include "public/cpio/test/nosql_database_client/test_gcp_nosql_database_client_options.h"

namespace google::scp::cpio {
/*! @copydoc NoSQLDatabaseClient
 */
class TestGcpNoSQLDatabaseClient : public NoSQLDatabaseClient {
 public:
  explicit TestGcpNoSQLDatabaseClient(
      const std::shared_ptr<TestGcpNoSQLDatabaseClientOptions>& options)
      : NoSQLDatabaseClient(options) {}
};
}  // namespace google::scp::cpio
