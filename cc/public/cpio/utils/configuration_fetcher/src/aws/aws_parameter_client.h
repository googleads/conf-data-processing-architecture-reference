/*
 * Copyright 2024 Google LLC
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

#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/parameter_client/src/parameter_client.h"

namespace google::scp::cpio {
/*! @copydoc ParameterClient
 */
class AwsParameterClient : public ParameterClient {
 public:
  explicit AwsParameterClient(
      const std::shared_ptr<ParameterClientOptions>& options,
      const std::shared_ptr<client_providers::InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor)
      : ParameterClient(options),
        instance_client_provider_(instance_client_provider),
        io_async_executor_(io_async_executor) {}

 protected:
  core::ExecutionResult CreateParameterClientProvider() noexcept override;

 private:
  std::shared_ptr<client_providers::InstanceClientProviderInterface>
      instance_client_provider_;
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
};
}  // namespace google::scp::cpio
