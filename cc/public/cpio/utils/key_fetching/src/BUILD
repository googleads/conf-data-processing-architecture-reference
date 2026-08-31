# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "key_fetching_metric_utils",
    srcs = ["key_fetching_metric_utils.cc"],
    hdrs = ["key_fetching_metric_utils.h"],
    deps = [
        "//cc/core/common/uuid/src:uuid_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/dual_writing_metric_client/interface:dual_writing_metric_client_interface",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "key_fetching_utils",
    srcs = ["key_fetching_utils.cc"],
    hdrs = ["key_fetching_utils.h"],
    deps = [
        "//cc/public/cpio/proto/private_key_service/v1:private_key_service_cc_proto",
        "//cc/public/cpio/utils/key_fetching/interface:key_fetcher_with_cache_interface",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "error_codes",
    hdrs = ["error_codes.h"],
    deps = [
        "//cc/core/interface:errors_lib",
        "//cc/public/core/interface:execution_result",
    ],
)

cc_library(
    name = "coordinator_key_fetcher_with_cache_base",
    srcs = ["coordinator_key_fetcher_with_cache_base.cc"],
    hdrs = [
        "coordinator_key_fetcher_with_cache_base.h",
    ],
    deps = [
        ":error_codes",
        ":key_fetching_metric_utils",
        ":key_fetching_utils",
        "//cc/core/common/auto_expiry_concurrent_map/src:auto_expiry_concurrent_map_lib",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/private_key_client",
        "//cc/public/cpio/utils/dual_writing_metric_client/interface:dual_writing_metric_client_interface",
        "//cc/public/cpio/utils/key_fetching/interface:key_fetcher_with_cache_interface",
        "//cc/public/cpio/utils/key_fetching/proto:encryption_key_prefetch_config_cc_proto",
        "//cc/public/cpio/utils/key_fetching/proto:key_coordinator_configuration_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "ondemand_key_fetcher_with_cache",
    srcs = ["ondemand_key_fetcher_with_cache.cc"],
    hdrs = [
        "ondemand_key_fetcher_with_cache.h",
    ],
    deps = [
        ":coordinator_key_fetcher_with_cache_base",
    ],
)

cc_library(
    name = "auto_refresh_key_fetcher_with_cache",
    srcs = ["auto_refresh_key_fetcher_with_cache.cc"],
    hdrs = [
        "auto_refresh_key_fetcher_with_cache.h",
    ],
    deps = [
        ":error_codes",
        ":key_fetching_metric_utils",
        ":key_fetching_utils",
        "//cc/core/common/concurrent_map/src:concurrent_map_lib",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/common/time_provider/src:time_provider_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/private_key_client",
        "//cc/public/cpio/utils/dual_writing_metric_client/interface:dual_writing_metric_client_interface",
        "//cc/public/cpio/utils/key_fetching/interface:key_fetcher_with_cache_interface",
        "//cc/public/cpio/utils/key_fetching/proto:key_coordinator_configuration_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "key_fetching",
    srcs = [
        "aws_wrapped_key_handler_with_cache.cc",
        "gcp_wrapped_key_handler_with_cache.cc",
        "wrapped_key_handler_with_cache_base.cc",
    ],
    hdrs = [
        "aws_wrapped_key_handler_with_cache.h",
        "gcp_wrapped_key_handler_with_cache.h",
        "wrapped_key_handler_with_cache_base.h",
        "wrapped_key_handler_with_cache_interface.h",
    ],
    deps = [
        ":error_codes",
        ":key_fetching_metric_utils",
        "//cc/core/common/auto_expiry_concurrent_map/src:auto_expiry_concurrent_map_lib",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/common/time_provider/src:time_provider_lib",
        "//cc/cpio/client_providers/kms_client_provider/src/aws:nontee_aws_kms_client_provider_no_factory",
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/kms_client",
        "//cc/public/cpio/utils:proto_utils",
        "//cc/public/cpio/utils/dual_writing_metric_client/interface:dual_writing_metric_client_interface",
        "//cc/public/cpio/utils/key_fetching/proto:cloud_wrapped_key_cc_proto",
        "//cc/public/cpio/utils/metric_instance/src:metric_instance",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "wrapped_key_handler_mock",
    testonly = True,
    hdrs = [
        "wrapped_key_handler_with_cache_mock.h",
    ],
    deps = [
        ":key_fetching",
        "//cc/public/core/interface:execution_result",
        "@com_google_googletest//:gtest_main",
    ],
)
