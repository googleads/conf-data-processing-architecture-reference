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

#ifndef SCP_CPIO_INTERFACE_AUTO_SCALING_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_AUTO_SCALING_CLIENT_TYPE_DEF_H_

#include <string>
#include <vector>

namespace google::scp::cpio {
/// Configurations for AutoScalingClient.
struct AutoScalingClientOptions {
  virtual ~AutoScalingClientOptions() = default;

  std::string instance_table_name;
  std::string gcp_spanner_instance_name;
  std::string gcp_spanner_database_name;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_AUTO_SCALING_CLIENT_TYPE_DEF_H_
