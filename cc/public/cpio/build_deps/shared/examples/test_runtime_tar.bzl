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

load("@io_bazel_rules_docker//docker/package_managers:download_pkgs.bzl", "download_pkgs")
load("@io_bazel_rules_docker//docker/package_managers:install_pkgs.bzl", "install_pkgs")
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_extract")

def test_runtime_tar(
        *,
        name):
    """Creates a tar example which can be passed into sdk_runtime_image.
    Exposes ${name}/socat_and_dependencies.tar
    """

    download_pkgs_name = "%s_download_pkgs" % name
    download_pkgs(
        name = download_pkgs_name,
        image_tar = "@debian_11//image",
        packages = [
            "socat",
        ],
        tags = ["manual"],
    )

    install_pkgs_name = "%s_install_pkgs" % name
    install_pkgs(
        name = install_pkgs_name,
        image_tar = "@debian_11//image",
        installables_tar = ":%s.tar" % download_pkgs_name,
        output_image_name = install_pkgs_name,
        tags = ["manual"],
    )

    socat_extract_commands = [
        "mkdir -p /extracted_files/usr/bin/",
        "mkdir -p /extracted_files/lib/x86_64-linux-gnu/",
        "mkdir -p /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/bin/socat /extracted_files/usr/bin/",
        "cp -r /usr/lib/x86_64-linux-gnu/libwrap.so.0 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libwrap.so.0) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/libutil.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libutil.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/libssl.so.1.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/libcrypto.so.1.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/libc.so.6 /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libc.so.6) /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/libnsl.so.2 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libnsl.so.2) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/libpthread.so.0 /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libpthread.so.0) /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/libdl.so.2 /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libdl.so.2) /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/libtirpc.so.3 /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libtirpc.so.3) /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/libgssapi_krb5.so.2 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libgssapi_krb5.so.2) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/libkrb5.so.3 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libkrb5.so.3) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/libk5crypto.so.3 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libk5crypto.so.3) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/libcom_err.so.2 /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libcom_err.so.2) /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/libkrb5support.so.0 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp -r /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libkrb5support.so.0) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libkeyutils.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libkeyutils.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/libresolv.so.2 /extracted_files/lib/x86_64-linux-gnu/",
        "cp -r /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libresolv.so.2) /extracted_files/lib/x86_64-linux-gnu/",
        "cd /extracted_files && tar -zcvf socat_and_dependencies.tar * && cp socat_and_dependencies.tar /",
    ]

    # Gather socat and all the dependencies it needs to run into a tar so that it can be copied into the runtime container
    container_run_and_extract(
        name = name,
        commands = socat_extract_commands,
        extract_file = "/socat_and_dependencies.tar",
        image = ":%s.tar" % install_pkgs_name,
        tags = ["manual"],
    )
