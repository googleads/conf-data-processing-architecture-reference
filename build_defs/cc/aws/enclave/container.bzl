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

load("@io_bazel_rules_docker//container:container.bzl", "container_image")
load("@io_bazel_rules_docker//docker/package_managers:download_pkgs.bzl", "download_pkgs")
load("@io_bazel_rules_docker//docker/package_managers:install_pkgs.bzl", "install_pkgs")
load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("//cc/process_launcher:helpers.bzl", "executable_struct_to_json_str")

# Note: use reproducible versions of these binaries.
_PROXY_BINARY_FILES = [
    Label("//cc/proxy/src:proxify"),
    Label("//cc/proxy/src:proxy_preload"),
    Label("//cc/proxy/src:socket_vendor"),
]

def cc_enclave_image(
        *,
        name,
        binary_target,
        binary_filename,
        additional_files = [],
        additional_tars = [],
        enclave_cmd_override = [],
        enclave_env_variables = {},
        binary_args = [],
        pkgs_to_install = []):
    """Creates a target for a Docker container with the necessary files for
    doing attested decryptions and communicating over the proxy for cc code.
    Exposes ${name}.tar

    Args:
      name: Name used for the generated container_image
      binary_target: Bazel target to include in the container
        (e.g. //cc:target)
      binary_filename: Filename of binary generated by bazel build prefixed
        with "/" (e.g. "/cc/target")" -- provided manually because there
        doesn't seem to be a straightforward way of getting the resulting
        filename from the target in a bazel macro.
      additional_files: Additional files to include in the container root.
      additional_tars: Additional files include in the container based on their
        paths within the tar.
      enclave_cmd_override: "cmd" parameter to use with the container_image.
        Only used for testing purposes.
      enclave_env_variables: Dict (string-to-string) of environment variables to
        be added to the enclave.
      binary_args: CLI args passed to the binary inside the enclave.
    """

    pkg_tar(
        name = "binary_tar",
        srcs = [
            binary_target,
        ],
        # Needs this flag to include generated dynamic libs,
        # such as nghttp2.so.
        include_runfiles = True,
        mode = "0755",
        ownername = "root.root",
        tags = ["manual"],
    )

    container_files = [
        Label("//cc/process_launcher:scp_process_launcher"),
        "@kmstool_enclave_cli//file",
    ] + additional_files

    container_tars = [
        ":binary_tar",
        Label("//operator/worker/aws:libnsm-tar"),  # This is to support using enclave KMS cli
    ] + additional_tars

    for b in _PROXY_BINARY_FILES:
        container_files.append(b)

    enclave_cmd = ["/scp_process_launcher"]  # This is to keep retry and get log messages more friendly

    proxify_cmd = {
        "args": [
            "/" + binary_filename,
        ] + binary_args,
        "file_name": "/proxify",
    }

    enclave_cmd.append(executable_struct_to_json_str(proxify_cmd))

    if len(enclave_cmd_override) > 0:
        enclave_cmd = enclave_cmd_override

    download_pkgs(
        name = "apt_pkgs_download",
        image_tar = "@debian_11//image",
        packages = pkgs_to_install,
        tags = ["manual"],
    )

    install_pkgs(
        name = "apt_pkgs_install",
        image_tar = "@debian_11//image",
        installables_tar = ":apt_pkgs_download.tar",
        output_image_name = "apt_pkgs_install",
        tags = ["manual"],
    )

    container_image(
        name = name,
        base = "apt_pkgs_install.tar",
        cmd = enclave_cmd,
        env = enclave_env_variables,
        files = container_files,
        tars = container_tars,
        tags = ["manual"],
    )
