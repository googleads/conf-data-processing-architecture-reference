# Copyright 2023 Google LLC
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
    name = "configuration_fetcher_mock",
    srcs = glob(
        [
            "*.h",
        ],
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/interface:logger_interface",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_proto",
        "//cc/public/cpio/utils/configuration_fetcher/interface:configuration_fetcher_interface",
    ],
)
