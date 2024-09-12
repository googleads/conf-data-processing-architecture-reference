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

# Mounts the source code into an AmazonLinux2023 docker container and performs a
# build of just the proxy dependencies. Used to ensure compatability with Amazon
# Linux 2's host libraries (specifically glibc).

load("@io_bazel_rules_docker//container:container.bzl", "container_image")

package(
    default_visibility = ["//visibility:public"],
)

container_image(
    name = "build_container_al2023",
    base = "@amazonlinux_2023//image",
)

genrule(
    name = "reproducible_proxy_outputs_al2023",
    srcs = [
        "//:source_code_tar",
        ":build_container_al2023.tar",
        "@bazelisk//file:bazelisk",
    ],
    outs = [
        "proxy",
    ],
    cmd = """
    export OUTPUT_DIR=`mktemp -d -t PROXY_BUILD_OUTPUTS_XXXXX`
    docker load < $(location :build_container_al2023.tar)
    docker run --rm \
      -v `realpath $(location //:source_code_tar)`:/source_code.tar \
      -v `realpath $(location @bazelisk//file:bazelisk)`:/bin/bazel \
      -v `realpath "$${OUTPUT_DIR}"`:/output \
      bazel/cc/proxy/parent_instance/aws/al2023:build_container_al2023 \
      bash -c '
        set -eux
        dnf install glibc-devel-2.34-52.amzn2023.0.3 clang-15.0.6-3.amzn2023.0.2 tar git -y > /dev/null
        tar xf /source_code.tar -C /
        cd /scp
        export CXX=/usr/bin/clang++-15
        export CC=/usr/bin/clang-15
        bazel build \
          //cc/proxy/src:proxy
        cp $$(bazel info bazel-bin)/cc/proxy/src/proxy /output
      '
    cp "$${OUTPUT_DIR}/proxy" $(location proxy)
    rm -r "$${OUTPUT_DIR}"
    """,
    tags = ["manual"],
)
