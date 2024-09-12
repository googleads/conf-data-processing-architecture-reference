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

load("//coordinator/keygeneration/gcp:build_defs.bzl", "keygeneration_container")

keygeneration_container(
    name = "key_generation_app_mp_gcp_image_release",
    registry = "us-docker.pkg.dev",
    repository = "admcloud-scp/docker-repo-dev/key_generation_app_mp_gcp",
    tag = "release",
)

# Find staging environment values in go/scp-internal-coordinator-envs-gcp
keygeneration_container(
    name = "key_generation_app_mp_gcp_image_staging",
    registry = "us-docker.pkg.dev",
    repository = "admcloud-scp/docker-repo-dev/key_generation_app_mp_gcp",
    tag = "staging",
)

keygeneration_container(
    name = "key_generation_app_mp_gcp_image_postsubmit",
    registry = "us-docker.pkg.dev",
    repository = "admcloud-scp/docker-repo-dev/key_generation_app_mp_gcp",
    tag = "postsubmit",
)

keygeneration_container(
    name = "key_generation_app_mp_gcp_image_dev",
    registry = "us-docker.pkg.dev",
    repository = "admcloud-scp/docker-repo-dev/key_generation_app_mp_gcp",
    tag = "dev",
)

keygeneration_container(
    name = "key_generation_app_mp_gcp_image_pre_release",
    registry = "us-docker.pkg.dev",
    repository = "gcp-ps-msmt-coord-primary/pre-release-coordinators/key_generation_app_mp_gcp",
    tag = "pre-release",
)
