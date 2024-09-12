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

load("//cc/public/cpio/build_deps/shared:build_time_image.bzl", "build_time_image")
load("//cc/public/cpio/build_deps/shared/examples:test_runtime_tar.bzl", "test_runtime_tar")

package(default_visibility = ["//visibility:public"])

build_time_image(
    name = "test_build_time_image",
)

test_runtime_tar(name = "runtime_test")
