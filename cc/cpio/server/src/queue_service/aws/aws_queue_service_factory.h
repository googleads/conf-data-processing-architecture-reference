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

#include "core/interface/config_provider_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "cpio/server/src/queue_service/queue_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc QueueServiceFactoryInterface
 */
class AwsQueueServiceFactory : public QueueServiceFactory {
 public:
  explicit AwsQueueServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : QueueServiceFactory(config_provider) {}

  /**
   * @brief Creates QueueClient.
   *
   * @return std::shared_ptr<client_providers::QueueClientProviderInterface>
   * created queue client.
   */
  std::shared_ptr<client_providers::QueueClientProviderInterface>
  CreateQueueClient() noexcept override;

 private:
  std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept override;
};
}  // namespace google::scp::cpio