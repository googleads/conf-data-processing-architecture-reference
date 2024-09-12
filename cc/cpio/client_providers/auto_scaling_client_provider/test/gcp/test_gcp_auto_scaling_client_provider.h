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

#include "cpio/client_providers/auto_scaling_client_provider/src/gcp/gcp_auto_scaling_client_provider.h"
#include "google/cloud/options.h"
#include "public/cpio/interface/auto_scaling_client/type_def.h"

namespace google::scp::cpio::client_providers {
class TestInstanceGroupManagersClientFactory
    : public InstanceGroupManagersClientFactory {
 public:
  cloud::Options CreateClientOptions(
      const std::shared_ptr<AutoScalingClientOptions>& options) noexcept
      override;

  virtual ~TestInstanceGroupManagersClientFactory() = default;
};

/// Provides InstanceDatabaseClientProvider for testing.
class TestInstanceDatabaseClientProviderFactory
    : public InstanceDatabaseClientProviderFactory {
 public:
  std::shared_ptr<InstanceDatabaseClientProviderInterface> CreateClient(
      const std::shared_ptr<AutoScalingClientOptions>& client_options,
      const std::shared_ptr<InstanceClientProviderInterface>& instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept override;
};
}  // namespace google::scp::cpio::client_providers
