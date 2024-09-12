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
    name = "gcp_kms_client_provider_test",
    size = "small",
    srcs = ["gcp_kms_client_provider_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/kms_client_provider/mock/gcp:gcp_kms_client_provider_mock",
        "//cc/cpio/client_providers/kms_client_provider/src/gcp:nontee_gcp_kms_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:kms",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tee_gcp_kms_client_provider_utils_test",
    size = "small",
    srcs = ["tee_gcp_kms_client_provider_utils_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/kms_client_provider/src/gcp:tee_gcp_kms_client_provider_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fake_gcp_kms_client_provider_lib",
    testonly = 1,
    srcs = [
        "fake_gcp_kms_client_provider.cc",
        "fake_gcp_kms_client_provider.h",
        "//cc/cpio/client_providers/interface:kms_client_provider_interface.h",
        "//cc/cpio/client_providers/kms_client_provider/src/gcp:gcp_kms_client_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//cc/cpio/client_providers/kms_client_provider/mock/gcp:gcp_kms_client_provider_mock",
        "//cc/cpio/client_providers/kms_client_provider/src/gcp:nontee_gcp_kms_client_provider_deps",
        "@com_github_googleapis_google_cloud_cpp//:kms",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fake_gcp_kms_client",
    testonly = 1,
    srcs = [
        "//cc/public/cpio/adapters/kms_client/src:kms_client_srcs",
    ],
    hdrs =
        [
            "//cc/public/cpio/interface/kms_client:kms_client_interface.h",
        ],
    deps = [
        ":fake_gcp_kms_client_provider_lib",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
        "//cc/public/cpio/adapters/common:adapter_utils",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/interface/kms_client:type_def",
        "//cc/public/cpio/proto/kms_service/v1:kms_service_cc_proto",
    ],
)
