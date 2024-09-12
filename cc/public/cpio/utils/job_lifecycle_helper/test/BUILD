# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//cc:scp_cc_internal_pkg"])

cc_test(
    name = "job_lifecycle_helper_test",
    size = "small",
    srcs = ["job_lifecycle_helper_test.cc"],
    flaky = True,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/auto_scaling_client_provider/src/gcp:gcp_auto_scaling_client_provider_lib",
        "//cc/cpio/client_providers/job_client_provider/src:job_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/mock/auto_scaling_client:auto_scaling_client_mock",
        "//cc/public/cpio/mock/job_client:job_client_mock",
        "//cc/public/cpio/proto/auto_scaling_service/v1:auto_scaling_service_cc_proto",
        "//cc/public/cpio/proto/job_service/v1:job_service_cc_proto",
        "//cc/public/cpio/utils/job_lifecycle_helper/src:job_lifecycle_helper",
        "//cc/public/cpio/utils/metric_instance/mock:metric_instance_mock",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
