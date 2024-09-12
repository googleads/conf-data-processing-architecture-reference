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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "configuration_fetcher",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts =
        select(
            {
                "//cc/public/cpio/interface:aws_cpio_lib": [
                    "-DAWS_CLIENT=1",
                ],
                "//cc/public/cpio/interface:gcp_cpio_lib": [
                    "-DGCP_CLIENT=1",
                ],
            },
            no_match_error = "Please build for AWS or GCP",
        ),
    deps = select(
        {
            "//cc/public/cpio/interface:aws_cpio_lib": [
                "//cc/cpio/client_providers/auth_token_provider/src/aws:aws_auth_token_provider_lib",
                "//cc/cpio/client_providers/instance_client_provider/src/aws:aws_instance_client_provider_lib",
                "//cc/public/cpio/utils/configuration_fetcher/src/aws",
            ],
            "//cc/public/cpio/interface:gcp_cpio_lib": [
                "//cc/core/http2_client/src:http2_client_lib",
                "//cc/cpio/client_providers/auth_token_provider/src/gcp:gcp_auth_token_provider_lib",
                "//cc/cpio/client_providers/instance_client_provider/src/gcp:gcp_instance_client_provider_lib",
                "//cc/cpio/client_providers/parameter_client_provider/src/gcp:gcp_parameter_client_provider_lib",
                "//cc/public/cpio/utils/configuration_fetcher/src/gcp",
            ],
        },
        no_match_error = "Please build for AWS or GCP",
    ) + [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/curl_client/src:http1_curl_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/server/interface:server_interface_lib",
        "//cc/cpio/server/interface/auto_scaling_service:auto_scaling_service_interface_lib",
        "//cc/cpio/server/interface/job_service:job_service_interface_lib",
        "//cc/cpio/server/interface/nosql_database_service:nosql_database_service_interface_lib",
        "//cc/cpio/server/interface/queue_service:queue_service_interface_lib",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/interface/instance_client",
        "//cc/public/cpio/interface/parameter_client",
        "//cc/public/cpio/proto/auto_scaling_service/v1:auto_scaling_service_cc_proto",
        "//cc/public/cpio/proto/common/v1:common_cc_proto",
        "//cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_proto",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
        "//cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_proto",
        "//cc/public/cpio/utils/configuration_fetcher/interface:configuration_fetcher_interface",
        "//cc/public/cpio/utils/job_lifecycle_helper/proto/v1:job_lifecycle_helper_cc_proto",
        "//cc/public/cpio/utils/sync_utils/src:sync_utils",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)
