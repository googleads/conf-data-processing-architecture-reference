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

load("@io_bazel_rules_docker//container:container.bzl", "container_image")
load("@io_bazel_rules_docker//docker/package_managers:download_pkgs.bzl", "download_pkgs")
load("@io_bazel_rules_docker//docker/package_managers:install_pkgs.bzl", "install_pkgs")
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit_layer")

package(
    default_visibility = ["//visibility:public"],
)

download_pkgs(
    name = "debian11_build_apt_pkgs_download",
    image_tar = "@debian_11//image",
    packages = [
        "ca-certificates",
        "clang-13",
        "git",
        "tar",
    ],
    tags = ["manual"],
)

install_pkgs(
    name = "debian11_build_apt_pkgs_install",
    image_tar = "@debian_11//image",
    installables_tar = ":debian11_build_apt_pkgs_download.tar",
    output_image_name = "debian11_build_apt_pkgs_install",
    tags = ["manual"],
)

container_image(
    name = "build_container_debian11_base",
    base = "debian11_build_apt_pkgs_install.tar",
    tags = ["manual"],
)

container_run_and_commit_layer(
    name = "container_to_build_debian11_bazel",
    commands = [
        "ln -s /usr/bin/clang-13 /usr/bin/clang",
        "ln -s /usr/bin/clang++-13 /usr/bin/clang++",
        "ln -s /usr/bin/clang-cpp-13 /usr/bin/clang-cpp",
    ],
    env = {
        "CC": "/usr/bin/clang",
        "CXX": "/usr/bin/clang++",
    },
    image = ":debian11_build_apt_pkgs_install.tar",
    tags = ["manual"],
)

container_image(
    name = "build_container_debian11",
    base = "build_container_debian11_base.tar",
    layers = [
        ":container_to_build_debian11_bazel",
    ],
    tags = ["manual"],
)

genrule(
    name = "reproducible_proxy_outputs_debian11",
    srcs = [
        "//:source_code_tar",
        ":build_container_debian11.tar",
        "@bazelisk//file:bazelisk",
    ],
    outs = [
        "libproxy_preload.so",
        "proxify",
        "socket_vendor",
    ],
    cmd = """
    export OUTPUT_DIR=`mktemp -d -t PROXY_BUILD_OUTPUTS_XXXXX`
    docker load < $(location :build_container_debian11.tar)
    docker run --rm \
      -v `realpath $(location //:source_code_tar)`:/source_code.tar \
      -v `realpath $(location @bazelisk//file:bazelisk)`:/bin/bazel \
      -v `realpath "$${OUTPUT_DIR}"`:/output \
      bazel/cc/proxy/tee/debian/debian11:build_container_debian11 \
      bash -c '
        set -eux
        tar xf /source_code.tar -C /
        cd /scp
        bazel build \
          //cc/proxy/src:proxify \
          //cc/proxy/src:proxy_preload \
          //cc/proxy/src:socket_vendor
        cp $$(bazel info bazel-bin)/cc/proxy/src/{libproxy_preload.so,proxify,socket_vendor} /output
      '
    cp "$${OUTPUT_DIR}/libproxy_preload.so" $(location libproxy_preload.so)
    cp "$${OUTPUT_DIR}/proxify" $(location proxify)
    cp "$${OUTPUT_DIR}/socket_vendor" $(location socket_vendor)
    rm -r "$${OUTPUT_DIR}"
    """,
    tags = ["manual"],
)
