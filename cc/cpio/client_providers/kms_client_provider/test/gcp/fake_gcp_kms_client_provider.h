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
#include <string>

#include "cpio/client_providers/kms_client_provider/src/gcp/gcp_kms_client_provider.h"

namespace google::scp::cpio::client_providers {
static constexpr char kPlaintext[] = "test_plaintext";

/*! @copydoc GcpKmsFactory
 */
class FakeGcpKmsFactory : public GcpKmsFactory {
 public:
  std::shared_ptr<GcpKeyManagementServiceClientInterface>
  CreateGcpKeyManagementServiceClient(
      const std::string& wip_provider,
      const std::string& service_account_to_impersonate) noexcept override;

  virtual ~FakeGcpKmsFactory() = default;
};
}  // namespace google::scp::cpio::client_providers
