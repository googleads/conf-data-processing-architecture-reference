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

#ifndef SCP_CPIO_INTERFACE_AWS_KMS_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_AWS_KMS_CLIENT_TYPE_DEF_H_

#include <string>

#include "public/cpio/interface/kms_client/type_def.h"

namespace google::scp::cpio {
/// KmsClientOptions for AWS.
struct AwsKmsClientOptions : public KmsClientOptions {
  /// Optional. If not set, fetch it using InstanceClient.
  /// If the code is not running on the corresponding cloud instance,
  /// InstanceClient will not work.
  std::string region;

  // If present, fetch the role credentials with web identity in the http
  // request.
  std::string target_audience_for_web_identity;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_AWS_KMS_CLIENT_TYPE_DEF_H_
