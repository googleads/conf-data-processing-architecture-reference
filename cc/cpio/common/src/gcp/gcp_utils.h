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

#include <string>

#include <grpcpp/support/status.h>

#include "google/cloud/status.h"

#include "error_codes.h"

namespace google::scp::cpio::common {

class GcpUtils {
 public:
  static core::ExecutionResult GcpErrorConverter(cloud::Status status) noexcept;
  static core::ExecutionResult GcpErrorConverter(grpc::Status status) noexcept;
  /**
   * @brief Creates credentials for attestion.
   *
   * @param wip_provider WIP provider.
   */
  static std::string CreateAttestedCredentials(
      const std::string& wip_provider) noexcept;
};
}  // namespace google::scp::cpio::common
