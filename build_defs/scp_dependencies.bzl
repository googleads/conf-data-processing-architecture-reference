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

load("//build_defs/cc:sdk.bzl", "sdk_dependencies")
load("//build_defs/shared:differential_privacy.bzl", "differential_privacy")
load("//build_defs/shared:google_java_format.bzl", "google_java_format")
load("//build_defs/shared:rpm.bzl", "rpm")

def scp_dependencies(protobuf_version, protobuf_repo_hash):
    sdk_dependencies(protobuf_version, protobuf_repo_hash)
    differential_privacy()
    google_java_format()
    rpm()
