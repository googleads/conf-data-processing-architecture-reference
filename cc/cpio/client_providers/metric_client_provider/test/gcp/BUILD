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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//cc:scp_cc_internal_pkg"])

cc_test(
    name = "gcp_metric_client_provider_test",
    size = "small",
    srcs = [
        "gcp_metric_client_provider_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//cc/cpio/client_providers/metric_client_provider/mock/gcp:gcp_metric_client_provider_mock",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/cpio/client_providers/metric_client_provider/src/gcp:gcp_metric_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "@com_github_googleapis_google_cloud_cpp//:monitoring",
        "@com_github_googleapis_google_cloud_cpp//:monitoring_mocks",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "gcp_metric_client_utils_test",
    size = "small",
    srcs = [
        "gcp_metric_client_utils_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/cpio/client_providers/metric_client_provider/src/gcp:gcp_metric_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:monitoring",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "fake_gcp_metric_client_provider_lib",
    testonly = 1,
    srcs = [
        "fake_gcp_metric_client_provider.cc",
        "fake_gcp_metric_client_provider.h",
        "//cc/cpio/client_providers/interface:metric_client_provider_interface.h",
        "//cc/cpio/client_providers/metric_client_provider/src/gcp:gcp_metric_client_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//cc/cpio/client_providers/metric_client_provider/src/gcp:gcp_metric_client_provider_deps",
        "@com_github_googleapis_google_cloud_cpp//:monitoring",
        "@com_github_googleapis_google_cloud_cpp//:monitoring_mocks",
    ],
)

cc_library(
    name = "fake_gcp_metric_client",
    testonly = 1,
    srcs = [
        "//cc/public/cpio/adapters/metric_client/src:metric_client_srcs",
    ],
    hdrs =
        [
            "//cc/public/cpio/interface/metric_client:metric_client_interface.h",
        ],
    deps = [
        ":fake_gcp_metric_client_provider_lib",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
        "//cc/public/cpio/adapters/common:adapter_utils",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/interface/metric_client:type_def",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
    ],
)
