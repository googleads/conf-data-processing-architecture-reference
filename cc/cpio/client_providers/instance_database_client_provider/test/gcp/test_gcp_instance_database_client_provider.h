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
#include <sstream>
#include <string>

#include "cpio/client_providers/common/test/gcp/test_gcp_database_factory.h"
#include "cpio/client_providers/instance_database_client_provider/src/gcp/gcp_instance_database_client_provider.h"
#include "google/cloud/options.h"
#include "public/cpio/test/common/test_gcp_database_client_options.h"

namespace google::scp::cpio::client_providers {
/// InstanceDatabaseClientOptions for testing on GCP.
struct TestGcpInstanceDatabaseClientOptions
    : public InstanceDatabaseClientOptions {
  TestGcpInstanceDatabaseClientOptions() = default;

  explicit TestGcpInstanceDatabaseClientOptions(
      const InstanceDatabaseClientOptions& options)
      : InstanceDatabaseClientOptions(options) {}

  TestGcpDatabaseClientOptions ToTestGcpDatabaseClientOptions() {
    return TestGcpDatabaseClientOptions(gcp_spanner_instance_name,
                                        gcp_spanner_database_name,
                                        impersonate_service_account, "");
  }

  std::string impersonate_service_account;
  virtual ~TestGcpInstanceDatabaseClientOptions() = default;
};

}  // namespace google::scp::cpio::client_providers
