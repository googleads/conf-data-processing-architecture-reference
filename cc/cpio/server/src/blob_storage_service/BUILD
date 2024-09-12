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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

# Need to be public visible in order to build SDK container in any other places.
package(default_visibility = ["//visibility:public"])

cc_library(
    name = "blob_storage_service_factory_lib",
    srcs = ["blob_storage_service_factory.cc"],
    hdrs = ["blob_storage_service_factory.h"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/server/interface/blob_storage_service:blob_storage_service_interface_lib",
        "//cc/cpio/server/interface/instance_service:instance_service_interface_lib",
        "//cc/public/cpio/proto/blob_storage_service/v1:blob_storage_service_cc_proto",
    ],
)

cc_binary(
    name = "blob_storage_service",
    srcs = ["blob_storage_service.cc"],
    copts = select(
        {
            "//cc/cpio/server/interface:aws_server": [
                "-DAWS_SERVER=1",
            ],
            "//cc/cpio/server/interface:gcp_server": [
                "-DGCP_SERVER=1",
            ],
            "//cc/cpio/server/interface:test_aws_server": [
                "-DTEST_AWS_SERVER=1",
            ],
            "//cc/cpio/server/interface:test_gcp_server": [
                "-DTEST_GCP_SERVER=1",
            ],
        },
        no_match_error = "Please build for AWS or GCP",
    ),
    deps = select(
        {
            "//cc/cpio/server/interface:aws_server": [
                "//cc/cpio/server/src/blob_storage_service/aws:aws_blob_storage_service_factory_lib",
            ],
            "//cc/cpio/server/interface:gcp_server": [
                "//cc/cpio/server/src/blob_storage_service/gcp:gcp_blob_storage_service_factory_lib",
            ],
            "//cc/cpio/server/interface:test_aws_server": [
                "//cc/cpio/server/src/blob_storage_service/test_aws:test_aws_blob_storage_service_factory_lib",
            ],
            "//cc/cpio/server/interface:test_gcp_server": [
                "//cc/cpio/server/src/blob_storage_service/test_gcp:test_gcp_blob_storage_service_factory_lib",
            ],
        },
        no_match_error = "Please build for AWS or GCP",
    ) + [
        "//cc:cc_base_include_dir",
        "//cc/core/grpc_server/callback/src:grpc_callback_lib",
        "//cc/core/network/src:core_network_lib",
        "//cc/cpio/server/src:service_utils_lib",
        "//cc/public/cpio/proto/blob_storage_service/v1:blob_storage_service_cc_proto",
        "//cc/public/cpio/proto/blob_storage_service/v1:blob_storage_service_grpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_reflection",
    ],
)
