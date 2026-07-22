# Copyright 2026 Google LLC
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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "key_fetching_metric_utils_test",
    srcs = [
        "key_fetching_metric_utils_test.cc",
    ],
    deps = [
        ":key_fetching_metric_matcher",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/common/src:cpio_utils_lib",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/dual_writing_metric_client/mock:dual_writing_metric_client_mock",
        "//cc/public/cpio/utils/key_fetching/src:key_fetching_metric_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "key_fetching_utils_test",
    srcs = [
        "key_fetching_utils_test.cc",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/common/src:cpio_utils_lib",
        "//cc/public/cpio/utils:proto_utils",
        "//cc/public/cpio/utils/key_fetching/src:key_fetching_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ondemand_key_fetcher_with_cache_test",
    srcs = [
        "ondemand_key_fetcher_with_cache_test.cc",
    ],
    deps = [
        ":key_fetching_metric_matcher",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/private_key_client:private_key_client_mock",
        "//cc/public/cpio/utils/dual_writing_metric_client/mock:dual_writing_metric_client_mock",
        "//cc/public/cpio/utils/key_fetching/src:ondemand_key_fetcher_with_cache",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "auto_refresh_key_fetcher_with_cache_test",
    srcs = [
        "auto_refresh_key_fetcher_with_cache_test.cc",
    ],
    deps = [
        ":key_fetching_metric_matcher",
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/private_key_client:private_key_client_mock",
        "//cc/public/cpio/utils:proto_utils",
        "//cc/public/cpio/utils/dual_writing_metric_client/mock:dual_writing_metric_client_mock",
        "//cc/public/cpio/utils/key_fetching/src:auto_refresh_key_fetcher_with_cache",
        "//cc/public/cpio/utils/key_fetching/src:key_fetching_metric_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "key_fetching_metric_matcher",
    testonly = 1,
    srcs = [
        "key_fetching_metric_matcher.cc",
    ],
    hdrs = ["key_fetching_metric_matcher.h"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/dual_writing_metric_client/mock:dual_writing_metric_client_mock",
        "//cc/public/cpio/utils/key_fetching/src:key_fetching_metric_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "key_fetching_test",
    testonly = True,
    srcs = [
        "aws_wrapped_key_handler_with_cache_test.cc",
        "gcp_wrapped_key_handler_with_cache_test.cc",
    ],
    deps = [
        ":key_fetching_metric_matcher",
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/mock/kms_client:kms_client_mock",
        "//cc/public/cpio/utils:proto_utils",
        "//cc/public/cpio/utils/dual_writing_metric_client/mock:dual_writing_metric_client_mock",
        "//cc/public/cpio/utils/key_fetching/src:error_codes",
        "//cc/public/cpio/utils/key_fetching/src:key_fetching",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
