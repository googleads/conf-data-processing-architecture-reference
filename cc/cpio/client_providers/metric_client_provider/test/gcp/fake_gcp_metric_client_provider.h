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
#include <string>

#include "cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_provider.h"

namespace google::scp::cpio::client_providers {
static constexpr char kPlaintext[] = "test_plaintext";

/*! @copydoc GcpMetricServiceClientFactory
 */
class FakeGcpMetricServiceClientFactory : public GcpMetricServiceClientFactory {
 public:
  std::shared_ptr<google::cloud::monitoring::MetricServiceClient>
  CreateClient() noexcept override;

  virtual ~FakeGcpMetricServiceClientFactory() = default;
};
}  // namespace google::scp::cpio::client_providers
