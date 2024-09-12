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

#include <gmock/gmock.h>

#include "cpio/client_providers/interface/instance_database_client_provider_interface.h"
#include "cpio/proto/instance_database_client.pb.h"

namespace google::scp::cpio::client_providers::mock {

/*! @copydoc InstanceDatabaseClientProviderInterface
 */
class MockInstanceDatabaseClientProvider
    : public InstanceDatabaseClientProviderInterface {
 public:
  MockInstanceDatabaseClientProvider() {
    ON_CALL(*this, Init)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
  }

  MOCK_METHOD(core::ExecutionResult, Init, (), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Run, (), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(
      void, GetInstanceByName,
      ((core::AsyncContext<
          cmrt::sdk::instance_database_client::GetInstanceByNameRequest,
          cmrt::sdk::instance_database_client::GetInstanceByNameResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      void, ListInstancesByStatus,
      ((core::AsyncContext<
          cmrt::sdk::instance_database_client::ListInstancesByStatusRequest,
          cmrt::sdk::instance_database_client::
              ListInstancesByStatusResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      void, UpdateInstance,
      ((core::AsyncContext<
          cmrt::sdk::instance_database_client::UpdateInstanceRequest,
          cmrt::sdk::instance_database_client::UpdateInstanceResponse>&)),
      (noexcept, override));
};

}  // namespace google::scp::cpio::client_providers::mock
