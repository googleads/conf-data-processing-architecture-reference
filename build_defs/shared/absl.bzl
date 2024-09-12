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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Note: these rules add a dependency on the golang toolchain and must be ordered
# after any `go_register_toolchains` calls in this file (or else the toolchain
# defined in io_bazel_rules_docker are used for future go toolchains)

def absl():
    # Bazel Skylib. Required by absl.
    maybe(
        http_archive,
        name = "bazel_skylib",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        ],
        sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
    )

    maybe(
        http_archive,
        name = "com_google_absl",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_absl/20230802.1.tar.gz",
            "https://github.com/abseil/abseil-cpp/archive/20230802.1.tar.gz",
        ],
        sha256 = "987ce98f02eefbaf930d6e38ab16aa05737234d7afbab2d5c4ea7adbe50c28ed",
        strip_prefix = "abseil-cpp-20230802.1",
    )
