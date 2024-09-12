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

#include "test_gcp_database_factory.h"

#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "public/cpio/test/common/test_gcp_database_client_options.h"

using google::cloud::MakeGoogleDefaultCredentials;
using google::cloud::MakeImpersonateServiceAccountCredentials;
using google::cloud::Options;
using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {
Options TestGcpDatabaseFactory::CreateClientOptions() noexcept {
  Options client_options = GcpDatabaseFactory::CreateClientOptions();
  auto test_options =
      dynamic_pointer_cast<TestGcpDatabaseClientOptions>(options_);
  if (!test_options->impersonate_service_account.empty()) {
    client_options.set<google::cloud::UnifiedCredentialsOption>(
        (MakeImpersonateServiceAccountCredentials(
            google::cloud::MakeGoogleDefaultCredentials(),
            test_options->impersonate_service_account)));
  }
  if (!test_options->spanner_endpoint_override.empty()) {
    client_options.set<google::cloud::EndpointOption>(
        test_options->spanner_endpoint_override);
  }

  return client_options;
}
}  // namespace google::scp::cpio::client_providers
