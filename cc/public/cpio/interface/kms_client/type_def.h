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

#ifndef SCP_CPIO_INTERFACE_KMS_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_KMS_CLIENT_TYPE_DEF_H_

#include <chrono>

namespace google::scp::cpio {
/// Configurations for KmsClient.
struct KmsClientOptions {
  virtual ~KmsClientOptions() = default;

  // Client cache config
  bool enable_gcp_kms_client_cache = false;
  std::chrono::seconds gcp_kms_client_cache_lifetime =
      std::chrono::seconds(3600 * 24);  // 1 day
  bool enable_aws_kms_client_cache = false;
  std::chrono::seconds aws_kms_client_cache_lifetime =
      std::chrono::seconds(60 * 70);  // 70 minutes

  // RPC retry config
  bool enable_gcp_kms_client_retries = false;
  std::chrono::milliseconds gcp_kms_client_retry_initial_interval =
      std::chrono::milliseconds(100);
  std::size_t gcp_kms_client_retry_total_retries = 4;

  // Temporary flag to use the new GCP Error Code Converter.
  bool enable_new_gcp_error_code_converter = false;

  // Flag to enable/disable pushing metrics for GCP KMS.
  bool enable_gcp_kms_metrics = false;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_KMS_CLIENT_TYPE_DEF_H_
