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

#ifndef SCP_CPIO_TEST_GCP_AUTO_SCALING_CLIENT_OPTIONS_H_
#define SCP_CPIO_TEST_GCP_AUTO_SCALING_CLIENT_OPTIONS_H_

#include <memory>
#include <string>

#include "public/cpio/interface/auto_scaling_client/type_def.h"

namespace google::scp::cpio {
/// AutoScalingClientOptions for testing on GCP.
struct TestGcpAutoScalingClientOptions : public AutoScalingClientOptions {
  virtual ~TestGcpAutoScalingClientOptions() = default;

  TestGcpAutoScalingClientOptions() = default;

  explicit TestGcpAutoScalingClientOptions(
      const AutoScalingClientOptions& options)
      : AutoScalingClientOptions(options) {}

  std::string impersonate_service_account;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_TEST_GCP_AUTO_SCALING_CLIENT_OPTIONS_H_
