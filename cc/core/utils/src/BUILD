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

package(default_visibility = ["//visibility:public"])

ERROR_UTILS_SRCS = [
    "error_utils.cc",
    "error_utils.h",
]

cc_library(
    name = "core_utils",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
        exclude = ERROR_UTILS_SRCS,
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/interface:type_def_lib",
        "//cc/public/core/interface:execution_result",
        "@boringssl//:crypto",
        "@com_github_curl_curl//:curl",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "error_utils",
    srcs = ERROR_UTILS_SRCS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:errors_lib",
        "//cc/public/core/interface:execution_result",
    ],
)
