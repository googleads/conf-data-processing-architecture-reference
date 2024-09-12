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

#include "cpio/client_providers/common/src/gcp/gcp_database_factory.h"
#include "google/cloud/options.h"
#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio::client_providers {
class TestGcpDatabaseFactory : public GcpDatabaseFactory {
 public:
  explicit TestGcpDatabaseFactory(
      std::shared_ptr<DatabaseClientOptions> options)
      : GcpDatabaseFactory(options) {}

  cloud::Options CreateClientOptions() noexcept override;

  virtual ~TestGcpDatabaseFactory() = default;
};
}  // namespace google::scp::cpio::client_providers
