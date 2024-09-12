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

def build_time_image(
        *,
        name,
        additional_pkgs_to_install = [],
        additional_commit_commands = [],
        additional_commit_env_variables = {},
        additional_env_variables = {},
        additional_files = [],
        additional_tars = []):
    """Creates the build time image.
    Exposes ${name}_commit.tar

    Args:
      name: Name of the bazel target.
      additional_pkgs_to_install: additional packages to be installed in the image.
      additional_commit_commands: additional commands to run in the build time container to install more things.
      additional_commit_env_variables: additional environment variables to setup for additional installation.
      additional_env_variables: Dict (string-to-string) of environment variables to
        be added to the container.
      additional_files: Additional files to include in the container root.
      additional_tars: Additional files include in the container based on their
        paths within the tar.
    """
    download_pkgs_name = "%s_download_pkgs" % name
    pkgs_to_install = [
        "autoconf",
        "clang-13",
        "curl",
        "default-jdk",
        "docker.io",
        "git",
        "openjdk-11-jre",
        "patch",
        "python-is-python3",
        "python3",
        "python3-apt",
        "python3-distutils",
        "python3-pip",
        "rsyslog",
        "socat",
        "wget",
        "zlib1g-dev",
    ] + additional_pkgs_to_install
    download_pkgs(
        name = download_pkgs_name,
        image_tar = "@debian_11//image",
        packages = pkgs_to_install,
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

    base_image_name = "%s_base" % name
    intall_pkgs_tar = "%s.tar" % install_pkgs_name
    container_image(
        name = base_image_name,
        base = intall_pkgs_tar,
        tags = ["manual"],
    )

    # NOTE: This bazel version should be kept up to date with the glinux version
    bazel_image_name = "%s_bazel" % name
    container_run_and_commit_layer(
        name = bazel_image_name,
        commands = [
            "curl -L https://github.com/bazelbuild/bazelisk/releases/download/v1.14.0/bazelisk-linux-amd64 -o /usr/local/bin/bazel",
            "chmod +x /usr/local/bin/bazel",
            "ln -s /usr/local/bin/clang-13 /usr/bin/clang",
            "ln -s /usr/local/bin/clang++-13 /usr/bin/clang++",
            "ln -s /usr/local/bin/clang-cpp-13 /usr/bin/clang-cpp",
            "pip3 install \"libclang~=13.0\"",
        ],
        env = {
            "CC": "/usr/bin/clang-13",
            "CXX": "/usr/bin/clang++-13",
        },
        image = ":%s" % intall_pkgs_tar,
        tags = ["manual"],
    )

    # Installing gcloud to push image to artifact registry
    gcloud_image_name = "%s_gcloud" % name
    container_run_and_commit_layer(
        name = gcloud_image_name,
        commands = [
            "curl -O https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-cli-426.0.0-linux-x86_64.tar.gz",
            "tar -xf google-cloud-cli-426.0.0-linux-x86_64.tar.gz",
            "./google-cloud-sdk/install.sh --quiet --path-update false",
            "ln -s /google-cloud-sdk/bin/gcloud /usr/local/bin/gcloud",
            "ln -s /google-cloud-sdk/bin/gsutil /usr/local/bin/gsutil",
            "ln -s /google-cloud-sdk/bin/docker-credential-gcloud /usr/local/bin/docker-credential-gcloud",
            "ln -s /google-cloud-sdk/bin/git-credential-gcloud.sh /usr/local/bin/git-credential-gcloud.sh",
        ],
        image = ":%s" % intall_pkgs_tar,
        tags = ["manual"],
    )

    # container_run_and_commit_layer cannot accept empty commands.
    if len(additional_commit_commands) == 0:
        additional_commit_commands = ["echo \"No additional commit commands\""]
    additional_image_name = "%s_additional" % name
    container_run_and_commit_layer(
        name = additional_image_name,
        commands = additional_commit_commands,
        env = additional_commit_env_variables,
        image = ":%s" % intall_pkgs_tar,
        tags = ["manual"],
    )

    container_image(
        name = name,
        base = "%s.tar" % base_image_name,
        entrypoint = ["/bin/bash", "-l", "-c"],
        env = additional_env_variables,
        files = additional_files,
        tars = additional_tars,
        layers = [
            ":%s" % bazel_image_name,
            ":%s" % gcloud_image_name,
            ":%s" % additional_image_name,
        ],
        tags = ["manual"],
    )
