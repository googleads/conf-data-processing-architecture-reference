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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def cc_utils():
    maybe(
        new_git_repository,
        name = "nlohmann_json",
        build_file = Label("//build_defs/cc/shared/build_targets:nlohmann.BUILD"),
        # Commits on Apr 6, 2022
        commit = "15fa6a342af7b51cb51a22599026e01f1d81957b",
        remote = "https://github.com/nlohmann/json.git",
    )

    maybe(
        git_repository,
        name = "oneTBB",
        # Commits on Apr 6, 2023, v2021.9.0
        commit = "a00cc3b8b5fb4d8115e9de56bf713157073ed68c",
        remote = "https://github.com/oneapi-src/oneTBB.git",
    )

    maybe(
        http_archive,
        # The name should match what is needed from google-cloud-cpp.
        name = "com_github_curl_curl",
        build_file = Label("//build_defs/cc/shared/build_targets:curl.BUILD"),
        # Matches the version needed from google-cloud-cpp.
        sha256 = "01ae0c123dee45b01bbaef94c0bc00ed2aec89cb2ee0fd598e0d302a6b5e0a98",
        strip_prefix = "curl-7.69.1",
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/curl.haxx.se/download/curl-7.69.1.tar.gz",
            "https://curl.haxx.se/download/curl-7.69.1.tar.gz",
        ],
    )

    maybe(
        new_git_repository,
        name = "moodycamel_concurrent_queue",
        build_file = Label("//build_defs/cc/shared/build_targets:moodycamel.BUILD"),
        # Commited Mar 20, 2022
        commit = "22c78daf65d2c8cce9399a29171676054aa98807",
        remote = "https://github.com/cameron314/concurrentqueue.git",
        shallow_since = "1647803790 -0400",
    )
