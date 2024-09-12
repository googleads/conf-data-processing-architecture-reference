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
#include <vector>

#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/kms_client/src/kms_client.h"
#include "public/cpio/interface/kms_client/aws/type_def.h"
#include "public/cpio/interface/kms_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/kms_service/v1/kms_service.pb.h"

namespace google::scp::cpio {
class AwsRoleCredentialsProviderFactory;
class AwsKmsClientProviderFactory;

/*! @copydoc KmsClient
 */
class AwsKmsClient : public KmsClient {
 public:
  explicit AwsKmsClient(
      const std::shared_ptr<AwsKmsClientOptions>& options,
      std::shared_ptr<AwsRoleCredentialsProviderFactory>
          role_credentials_provider_factory =
              std::make_shared<AwsRoleCredentialsProviderFactory>(),
      std::shared_ptr<AwsKmsClientProviderFactory> kms_client_provider_factory =
          std::make_shared<AwsKmsClientProviderFactory>())
      : KmsClient(std::dynamic_pointer_cast<KmsClientOptions>(options)),
        role_credentials_provider_factory_(role_credentials_provider_factory),
        kms_client_provider_factory_(kms_client_provider_factory) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

 private:
  std::shared_ptr<client_providers::RoleCredentialsProviderInterface>
      role_credentials_provider_;
  std::shared_ptr<AwsRoleCredentialsProviderFactory>
      role_credentials_provider_factory_;
  std::shared_ptr<AwsKmsClientProviderFactory> kms_client_provider_factory_;
};

class AwsRoleCredentialsProviderFactory {
 public:
  virtual std::shared_ptr<client_providers::RoleCredentialsProviderInterface>
  Create(
      const std::string& region,
      const std::shared_ptr<client_providers::InstanceClientProviderInterface>&,
      const std::shared_ptr<core::AsyncExecutorInterface>&,
      const std::shared_ptr<core::AsyncExecutorInterface>&,
      const std::shared_ptr<client_providers::AuthTokenProviderInterface>&);
};

class AwsKmsClientProviderFactory {
 public:
  virtual std::shared_ptr<client_providers::KmsClientProviderInterface> Create(
      const std::shared_ptr<AwsKmsClientOptions>&,
      const std::shared_ptr<
          client_providers::RoleCredentialsProviderInterface>&,
      const std::shared_ptr<core::AsyncExecutorInterface>&,
      const std::shared_ptr<core::AsyncExecutorInterface>&);
};
}  // namespace google::scp::cpio
