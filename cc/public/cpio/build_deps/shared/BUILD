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

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

exports_files(["build_sdk_runtime_image.bzl"])

bool_flag(
    name = "reproducible_build",
    build_setting_default = True,
)

config_setting(
    name = "reproducible_build_config",
    flag_values = {
        ":reproducible_build": "True",
    },
)

config_setting(
    name = "non_reproducible_build_config",
    flag_values = {
        ":reproducible_build": "False",
    },
)

# Collection of helper scripts to support reproducible build.
pkg_files(
    name = "reproducible_build_scripts_pkg",
    srcs = [
        "reproducible_build.sh",
    ],
    prefix = "reproducible_build_utils",
)
