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

#ifndef SCP_CPIO_TEST_GCP_NOSQL_DATABASE_CLIENT_OPTIONS_H_
#define SCP_CPIO_TEST_GCP_NOSQL_DATABASE_CLIENT_OPTIONS_H_

#include <memory>
#include <string>

#include "public/cpio/interface/nosql_database_client/type_def.h"
#include "public/cpio/test/common/test_gcp_database_client_options.h"

namespace google::scp::cpio {
/// NoSQLDatabaseClientOptions for testing on GCP.
struct TestGcpNoSQLDatabaseClientOptions : public NoSQLDatabaseClientOptions {
  TestGcpNoSQLDatabaseClientOptions() = default;

  explicit TestGcpNoSQLDatabaseClientOptions(
      const NoSQLDatabaseClientOptions& options)
      : NoSQLDatabaseClientOptions(options) {}

  TestGcpDatabaseClientOptions ToTestGcpDatabaseClientOptions() {
    return TestGcpDatabaseClientOptions(
        gcp_spanner_instance_name, gcp_spanner_database_name,
        impersonate_service_account, spanner_endpoint_override);
  }

  std::string impersonate_service_account;
  std::string spanner_endpoint_override;
  virtual ~TestGcpNoSQLDatabaseClientOptions() = default;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_TEST_GCP_NOSQL_DATABASE_CLIENT_OPTIONS_H_
