# Copyright 2024 Google LLC
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

package(default_visibility = ["//cc:scp_cc_internal_pkg"])

exports_files([
    "aws_kms_client.cc",
    "aws_kms_client.h",
    "aws_kms_client_factory.cc",
])

filegroup(
    name = "aws_kms_client_srcs",
    srcs = [
        ":aws_kms_client.cc",
        ":aws_kms_client.h",
        ":aws_kms_client_factory.cc",
    ],
)
