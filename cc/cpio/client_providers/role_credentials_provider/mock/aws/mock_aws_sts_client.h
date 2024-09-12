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

#include <functional>
#include <memory>
#include <string>

#include <aws/sts/STSClient.h>

namespace google::scp::cpio::client_providers::mock {

class MockSTSClient : public Aws::STS::STSClient {
 public:
  MOCK_METHOD(void, AssumeRoleAsync,
              (const Aws::STS::Model::AssumeRoleRequest&,
               const Aws::STS::AssumeRoleResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(
      void, AssumeRoleWithWebIdentityAsync,
      (const Aws::STS::Model::AssumeRoleWithWebIdentityRequest&,
       const Aws::STS::AssumeRoleWithWebIdentityResponseReceivedHandler&,
       const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
      (const, override));
};
}  // namespace google::scp::cpio::client_providers::mock
