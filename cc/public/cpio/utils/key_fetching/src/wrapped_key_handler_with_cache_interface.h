/*
 * Copyright 2026 Google LLC
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

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "cc/core/interface/service_interface.h"
#include "cc/public/cpio/utils/key_fetching/proto/cloud_wrapped_key.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio {

struct WrappedKeyHandlerOptions {
  bool enable_decryption_lock = true;
  std::chrono::seconds key_cache_lifetime = std::chrono::seconds(60 * 30);
  std::chrono::seconds key_failure_cache_lifetime =
      std::chrono::seconds(5 * 60);
  std::chrono::milliseconds key_decryption_waiting_timeout =
      std::chrono::milliseconds(2000);  // 2s
  // Only disable for benchmark testing.
  bool enable_cache = true;

  std::vector<std::string> image_signature_key_ids;
  std::string aws_target_audience_for_web_identity;
};

/// @brief Interface to get and cache Gcp Wrapped Keys from a key management
/// service.
class WrappedKeyHandlerWithCacheInterface
    : public google::scp::core::ServiceInterface {
 public:
  virtual ~WrappedKeyHandlerWithCacheInterface() = default;

  /// @brief Obtain decrypted DEK corresponding to this Wrapped Key
  /// @return decrypted DEK as string or error.
  virtual google::scp::core::ExecutionResultOr<std::string> GetKey(
      const google::cmrt::sdk::v1::CloudWrappedKey& wrapped_key) noexcept = 0;
};

}  // namespace google::scp::cpio
