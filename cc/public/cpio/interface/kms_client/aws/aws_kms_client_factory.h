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

#ifndef SCP_CPIO_INTERFACE_AWS_KMS_CLIENT_FACTORY_H_
#define SCP_CPIO_INTERFACE_AWS_KMS_CLIENT_FACTORY_H_

#include <memory>
#include <string>

#include "public/cpio/interface/kms_client/kms_client_interface.h"
#include "public/cpio/interface/kms_client/type_def.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Factory to create AwsKmsClient.
 *
 * This factory will create a AWS KMS client with running outside TEE version.
 * It should be only used in the mix-cloud scenarios like using AWS KMS client
 * in GCP.
 *
 * You should use the bazel flags //cc/public/cpio/interface:platform and
 * //cc/public/cpio/interface:run_inside_tee to control which cloud and whether
 * run inside TEE, instead of using this factory.
 *
 */
class AwsKmsClientFactory {
 public:
  /**
   * @brief Creates KmsClient.
   *
   * @return std::unique_ptr<KmsClient> KmsClient object.
   */
  static std::unique_ptr<KmsClientInterface> Create(
      AwsKmsClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_AWS_KMS_CLIENT_FACTORY_H_
