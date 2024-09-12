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

#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/instance_client/src/instance_client.h"

namespace google::scp::cpio {
/*! @copydoc InstanceClientInterface
 */
class AwsInstanceClient : public InstanceClient {
 public:
  explicit AwsInstanceClient(
      const std::shared_ptr<InstanceClientOptions>& options,
      const std::shared_ptr<client_providers::InstanceClientProviderInterface>&
          instance_client_provider)
      : InstanceClient(options) {
    instance_client_provider_ = instance_client_provider;
  }

 protected:
  core::ExecutionResult CreateInstanceClientProvider() noexcept override;
};
}  // namespace google::scp::cpio
