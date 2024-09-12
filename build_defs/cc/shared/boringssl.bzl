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

def boringssl():
    maybe(
        http_archive,
        name = "boringssl",
        sha256 = "96dd8b9be49a9954db8e3e6f75eae4c1dca1df1081b8598db4166671cfcff445",
        strip_prefix = "boringssl-3a138e43694c381cbd3d35f3237afed5724a67e8",
        url = "https://github.com/google/boringssl/archive/3a138e43694c381cbd3d35f3237afed5724a67e8.zip",
    )
