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

load("@io_bazel_rules_docker//container:container.bzl", "container_push")
load("//cc/public/cpio/build_deps/shared:build_sdk_runtime_image.bzl", "build_sdk_runtime_image")

def gcp_sdk_lib_container(
        name,
        client_binaries,
        image_repository,
        image_registry,
        image_tag,
        inside_tee = True,
        recover_client_binaries = True,
        additional_env_variables = {},
        additional_files = [],
        additional_tars = [],
        ports = [],
        sdk_cmd_override = []):
    """
    Creates a runnable target for pubshing a GCP SDK image to gcloud.
    The image name is the given name, and the image will be pushed to the given
    image_repository in the given image_registry with the given image_tag.

    To push the image, `bazel run` the provided name of this target.
    """

    sdk_container_name = "%s_cmrt_sdk_lib" % name
    build_sdk_runtime_image(
        name = sdk_container_name,
        client_binaries = client_binaries,
        inside_tee = inside_tee,
        platform = "gcp",
        additional_env_variables = additional_env_variables,
        recover_client_binaries = recover_client_binaries,
        additional_files = additional_files,
        additional_tars = additional_tars,
        ports = ports,
        sdk_cmd_override = sdk_cmd_override,
    )

    # Push image to GCP
    reproducible_container_name = "%s_reproducible_container" % sdk_container_name
    container_push(
        name = name,
        format = "Docker",
        image = select(
            {
                Label("//cc/public/cpio/build_deps/shared:reproducible_build_config"): ":%s.tar" % reproducible_container_name,
                Label("//cc/public/cpio/build_deps/shared:non_reproducible_build_config"): ":%s_container.tar" % sdk_container_name,
            },
            no_match_error = "Please provide reproducible_build flag",
        ),
        registry = image_registry,
        repository = image_repository,
        tag = image_tag,
        tags = ["manual"],
    )
