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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def bazel_rules_cpp():
    maybe(
        http_archive,
        name = "rules_cc",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/rules_cc/rules_cc-0.0.1.tar.gz",
            "https://github.com/bazelbuild/rules_cc/releases/download/0.0.1/rules_cc-0.0.1.tar.gz",
        ],
        sha256 = "4dccbfd22c0def164c8f47458bd50e0c7148f3d92002cdb459c2a96a68498241",
    )

    maybe(
        git_repository,
        name = "rules_foreign_cc",
        # version 0.8.0
        commit = "e208deeae394c2aba5531d1f4edbba82d2e7430c",
        remote = "https://github.com/bazelbuild/rules_foreign_cc.git",
        shallow_since = "1650286188 -0700",
    )
