# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//cc:scp_cc_internal_pkg"])

cc_library(
    name = "test_aws_instance_service_factory_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/network/src:core_network_lib",
        "//cc/cpio/client_providers/instance_client_provider/test/aws:test_aws_instance_client_provider_lib",
        "//cc/cpio/server/src:service_utils_lib",
        "//cc/cpio/server/src/instance_service/aws:aws_instance_service_factory_lib",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
    ],
)
