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

#include "cpio/server/src/nosql_database_service/aws/aws_nosql_database_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc NoSQLDatabaseServiceFactoryInterface
 */
class TestAwsNoSQLDatabaseServiceFactory
    : public AwsNoSQLDatabaseServiceFactory {
 public:
  using AwsNoSQLDatabaseServiceFactory::AwsNoSQLDatabaseServiceFactory;

 protected:
  std::shared_ptr<InstanceServiceFactoryOptions>
  CreateInstanceServiceFactoryOptions() noexcept override;

 private:
  std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept override;
};

}  // namespace google::scp::cpio