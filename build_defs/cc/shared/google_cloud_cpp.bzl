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

def import_google_cloud_cpp():
    # Load the googleapis dependency for gcloud.
    maybe(
        http_archive,
        name = "com_github_googleapis_google_cloud_cpp",
        sha256 = "8cb87ec2947e691a7f8631877e78453bcfda51b3d3cd4940794a376741888d37",
        strip_prefix = "google-cloud-cpp-2.17.0",
        url = "https://github.com/googleapis/google-cloud-cpp/archive/v2.17.0.tar.gz",
    )
