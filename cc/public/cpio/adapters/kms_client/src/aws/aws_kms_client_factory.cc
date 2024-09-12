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

#include "public/cpio/interface/kms_client/aws/aws_kms_client_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "aws_kms_client.h"

using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;

namespace google::scp::cpio {
unique_ptr<KmsClientInterface> AwsKmsClientFactory::Create(
    AwsKmsClientOptions options) {
  return make_unique<AwsKmsClient>(
      make_shared<AwsKmsClientOptions>(move(options)));
}
}  // namespace google::scp::cpio
