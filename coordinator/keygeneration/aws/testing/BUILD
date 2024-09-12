# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("//coordinator/keygeneration/aws:build_defs.bzl", "keygeneration_ami")

string_flag(
    name = "ami_name_flag_continuous",
    build_setting_default = "scp-keygeneration-enclave-continuous",
)

string_flag(
    name = "ami_name_flag_postsubmit_e2e",
    build_setting_default = "scp-keygeneration-enclave-postsubmit-e2e",
)

string_flag(
    name = "ami_name_flag_staging",
    build_setting_default = "scp-keygeneration-enclave-staging",
)

string_flag(
    name = "ami_name_flag_mp_tar_release",
    build_setting_default = "scp-keygeneration-enclave-mp-tar-release",
)

string_flag(
    name = "ami_name_flag_mp_tar_release_branch",
    build_setting_default = "scp-keygeneration-enclave-mp-tar-release-branch",
)

string_flag(
    name = "ami_region_flag_mp_tar_release_branch",
    build_setting_default = "us-east-1",
)

keygeneration_ami(
    name = "scp_keygeneration_ami_continuous",
    ami_name = ":ami_name_flag_continuous",
    aws_region = "us-east-1",
    debug_mode = True,
)

keygeneration_ami(
    name = "scp_keygeneration_ami_postsubmit_e2e",
    ami_name = ":ami_name_flag_postsubmit_e2e",
    aws_region = "us-east-1",
    debug_mode = True,
)

keygeneration_ami(
    name = "scp_keygeneration_ami_staging",
    ami_name = ":ami_name_flag_staging",
    aws_region = "us-east-1",
    debug_mode = True,
)

keygeneration_ami(
    name = "scp_keygeneration_ami_mp_tar_release",
    ami_name = ":ami_name_flag_mp_tar_release",
    aws_region = "us-east-1",
    debug_mode = True,
)

keygeneration_ami(
    name = "scp_keygeneration_ami_mp_tar_release_branch",
    ami_name = ":ami_name_flag_mp_tar_release_branch",
    aws_region_override = ":ami_region_flag_mp_tar_release_branch",
    debug_mode = True,
)
