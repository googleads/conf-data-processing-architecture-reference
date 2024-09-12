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
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit_layer", "container_run_and_extract")
load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("//cc/process_launcher:helpers.bzl", "executable_struct_to_json_str")

_PROXY_BINARY_FILES = [
    Label("//cc/proxy/src:proxify"),
    Label("//cc/proxy/src:proxy_preload"),
    Label("//cc/proxy/src:socket_vendor"),
]

def sdk_runtime_image(
        *,
        name,
        platform,
        inside_tee,
        sdk_binaries = {},
        recover_client_binaries = True,
        recover_sdk_binaries = True,
        client_binaries = {},
        additional_env_variables = {},
        additional_files = [],
        additional_tars = [],
        ports = [],
        sdk_cmd_override = []):
    """Creates a target for a Docker container with the necessary files for
    doing cpio services for cc code.
    Exposes ${name}.tar

    Args:
      name: Name used for the generated container_image
      sdk_binaries: Bazel target to include in the container
        with its environment variables. This is a dict in the form:
        {
            "service name": {
                "env1_name":"env1_value",
                "env2_name":"env2_value",
            }
        }
      platform: platform to run this container on (aws/gcp)
      inside_tee: whether this container is running inside tee.
      recover_client_binaries: if True, auto-restart the client binaries in the container if they failed.
      recover_sdk_binaries: if True, auto-restart the SDK binaries in the container if they failed.
      client_binaries: Dict of bazel targets of client binaries with executing command.
        For some languages, the executing command is not only the executable target.
        For example, for Java, the binary_file name is binary_target_deploy.jar, and the
        executing command is "/usr/bin/java -jar /binary_file".
        Example usage:
        {
            "client_binary_target" : "/binary_file arg1 arg2"],
        }
        If no executing args needed and the binary file is the same as the binary target,
        we can omit the value in the dictionary.
      additional_env_variables: Dict (string-to-string) of environment variables to
        be added to the container.
      additional_files: Additional files to include in the container root.
      additional_tars: Additional files include in the container based on their
        paths within the tar.
      ports: ports to expose in the docker container. If not specified, 80 is expose as default.
      sdk_cmd_override: "cmd" parameter to use with the container_image.
        Only used for testing purposes.
    """

    sdk_targets = [Label("//cc/cpio/server/src/" + target + ":" + target) for target in sdk_binaries.keys()]

    # Should not add Label here, because the client target may be from another repo, and it will look for the target in SCP respo
    # if adding Label here.
    client_targets = [target for target in client_binaries.keys()] if len(client_binaries) > 0 else []
    binary_targets = sdk_targets
    binary_targets += client_targets
    binary_tar = "%s_binary" % name
    pkg_tar(
        name = binary_tar,
        srcs = binary_targets,
        # Needs this flag to include generated dynamic libs,
        # such as nghttp2.so.
        include_runfiles = True,
        mode = "0755",
        ownername = "root.root",
        tags = ["manual"],
    )
    container_files = []
    container_files = [
        Label("//cc/process_launcher:scp_process_launcher"),
    ] + additional_files

    container_tars = [
        ":%s" % binary_tar,
    ] + additional_tars

    if platform == "aws" and inside_tee:
        for b in _PROXY_BINARY_FILES:
            container_files.append(b)

        # This is to support using enclave KMS cli
        container_files.append("@kmstool_enclave_cli//file")
        container_tars.append(Label("//operator/worker/aws:libnsm-tar"))

    # Recreate the binary targets list to generate executing cmd.
    client_targets_with_label = [Label(target) for target in client_binaries.keys()] if len(client_binaries) > 0 else []
    client_targets_dict_with_label = {Label(k): v for k, v in client_binaries.items()} if len(client_binaries) > 0 else {}

    # Don't reuse sdk_targets because it has the same value with binary_targets already.
    binary_targets_with_label = [Label("//cc/cpio/server/src/" + target + ":" + target) for target in sdk_binaries.keys()]
    binary_targets_with_label += client_targets_with_label
    sdk_cmd = sdk_cmd_override
    if len(sdk_cmd_override) <= 0:
        sdk_cmd = ["/scp_process_launcher"]  # This is to keep retry and get log messages more friendly
        for target in binary_targets_with_label:
            restart = recover_sdk_binaries
            if (target in client_targets_with_label):
                restart = recover_client_binaries
            execute_command = client_targets_dict_with_label.get(target, [])
            if (not execute_command):
                execute_command = ["/" + target.name]
            if platform == "aws" and inside_tee:
                executable = {
                    "args": execute_command,
                    "file_name": "/proxify",
                    "restart": restart,
                }
            else:
                executable = {
                    "args": execute_command[1:],
                    "file_name": execute_command[0],
                    "restart": restart,
                }
            sdk_cmd.append(executable_struct_to_json_str(executable))

    rsyslog_daemon = {
        "args": [
            # Run in the foreground
            "-n",
        ],
        "file_name": "/usr/sbin/rsyslogd",
    }
    sdk_cmd.append(executable_struct_to_json_str(rsyslog_daemon))

    # Log the binaries we are trying to bring up
    print(sdk_cmd)

    ############# Prepare Debian11 image starts ###############
    download_pkgs_name = "%s_download_pkgs" % name
    download_pkgs(
        name = download_pkgs_name,
        image_tar = "@debian_11//image",
        packages = [
            "rsyslog",
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

    rsyslog_extract_commands = [
        "mkdir -p /extracted_files/usr/sbin/",
        "mkdir -p /extracted_files/etc/",
        "mkdir -p /extracted_files/lib/x86_64-linux-gnu/",
        "mkdir -p /extracted_files/usr/lib/x86_64-linux-gnu/",
        "mkdir -p /extracted_files/usr/lib/x86_64-linux-gnu/rsyslog/",
        "cp /usr/sbin/rsyslogd /extracted_files/usr/sbin/",
        "cp /etc/rsyslog.conf /extracted_files/etc/",
        "cp /lib/x86_64-linux-gnu/libz.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libz.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libpthread.so.0 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libpthread.so.0) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libdl.so.2 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libdl.so.2) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libestr.so.0 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libestr.so.0) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libfastjson.so.4 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libfastjson.so.4) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libsystemd.so.0 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libsystemd.so.0) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libuuid.so.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libuuid.so.1) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libc.so.6 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libc.so.6) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/librt.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/librt.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/liblzma.so.5 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/liblzma.so.5) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libzstd.so.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libzstd.so.1) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/liblz4.so.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/liblz4.so.1) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libgcrypt.so.20 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libgcrypt.so.20) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libgpg-error.so.0 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libgpg-error.so.0) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/rsyslog/* /extracted_files/usr/lib/x86_64-linux-gnu/rsyslog/",
        "cd /extracted_files && tar -zcvf rsyslog_and_dependencies.tar * && cp rsyslog_and_dependencies.tar /",
    ]

    # Gather rsyslog and all the dependencies it needs to run into a tar so that it can be copied into the runtime container
    rsyslog_extract_name = "%s_rsyslog_extract" % name
    container_run_and_extract(
        name = rsyslog_extract_name,
        commands = rsyslog_extract_commands,
        extract_file = "/rsyslog_and_dependencies.tar",
        image = ":%s.tar" % install_pkgs_name,
        tags = ["manual"],
    )
    ############# Prepare Debian11 image ends ###############

    sdk_runtime_image_files = [
        ":%s/rsyslog_and_dependencies.tar" % rsyslog_extract_name,
    ]

    # Base image to extract the dependencies in
    runtime_base_image = "%s_runtime_base_image" % name
    container_image(
        name = runtime_base_image,
        base = "@java_debug_runtime//image",
        files = sdk_runtime_image_files,
        tags = ["manual"],
    )

    runtime_dependencies_layer_commands = [
        "addgroup adm",
        "ln -s /busybox/sh /bin/sh",
        "cd / && tar -xf rsyslog_and_dependencies.tar",
        "rm /rsyslog_and_dependencies.tar",
    ]
    runtime_dependencies_layer = "%s_runtime_dependencies_layer" % name
    container_run_and_commit_layer(
        name = runtime_dependencies_layer,
        commands = runtime_dependencies_layer_commands,
        docker_run_flags = [
            "--entrypoint=''",
            "--user root",
        ],
        image = "%s.tar" % runtime_base_image,
        tags = ["manual"],
    )

    labels = {}
    if platform == "gcp" and inside_tee:
        labels = {
            "tee.launch_policy.allow_cmd_override": "false",
            "tee.launch_policy.log_redirect": "always",
            "tee.launch_policy.monitoring_memory_allow": "always",
        }
    container_env = dict(additional_env_variables)
    for d in sdk_binaries.values():
        container_env.update(d)

    # Differentiate no ports configured and empty ports/default port configured
    # by using if else here.
    if len(ports) == 0:
        container_image(
            name = name,
            base = "@java_debug_runtime//image",
            cmd = sdk_cmd,
            entrypoint = None,
            env = container_env,
            layers = [":%s" % runtime_dependencies_layer],
            files = container_files,
            tars = container_tars,
            tags = ["manual"],
            user = "root",
            empty_dirs = ["/tmp"],
            labels = labels,
        )
    else:
        container_image(
            name = name,
            base = "@java_debug_runtime//image",
            cmd = sdk_cmd,
            entrypoint = None,
            env = container_env,
            layers = [":%s" % runtime_dependencies_layer],
            files = container_files,
            tars = container_tars,
            tags = ["manual"],
            ports = ports,
            user = "root",
            empty_dirs = ["/tmp"],
            labels = labels,
        )
