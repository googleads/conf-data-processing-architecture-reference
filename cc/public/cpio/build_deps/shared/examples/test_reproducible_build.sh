#!/bin/bash
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

# Run the script from the root of the repo.
code_repo_dir=$(git rev-parse --show-toplevel)

source "${code_repo_dir}/cc/public/cpio/build_deps/shared/reproducible_build.sh"

setup_build_time_container "cc/public/cpio/build_deps/shared/examples:test_build_time_image.tar" "bazel-bin/cc/public/cpio/build_deps/shared/examples/test_build_time_image.tar"

build_command="bazel build --//cc/public/cpio/build_deps/shared:reproducible_build=False --//cc/public/cpio/interface:platform=gcp --//cc/public/cpio/interface:run_inside_tee=True //javatests/com/google/scp/e2e/cmrtworker/gcp:test_gcp_sdk_server_container"
# Build runtime test tar.
# build_command="bazel build cc/public/cpio/build_deps/shared/examples:runtime_test"
run_within_build_time_container "bazel/cc/public/cpio/build_deps/shared/examples:test_build_time_image" "$code_repo_dir" "$build_command"
