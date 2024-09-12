# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "aggregate_metric_test",
    size = "small",
    srcs = ["aggregate_metric_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/metric_instance/interface:metric_instance_interface",
        "//cc/public/cpio/utils/metric_instance/interface:type_def",
        "//cc/public/cpio/utils/metric_instance/mock:metric_instance_mock",
        "//cc/public/cpio/utils/metric_instance/src:metric_instance",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "simple_metric_test",
    size = "small",
    srcs = ["simple_metric_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/metric_instance/interface:metric_instance_interface",
        "//cc/public/cpio/utils/metric_instance/interface:type_def",
        "//cc/public/cpio/utils/metric_instance/src:metric_instance",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "metric_utils_test",
    size = "small",
    srcs = ["metric_utils_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/metric_instance/mock:metric_instance_mock",
        "//cc/public/cpio/utils/metric_instance/src:metric_instance",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "metric_instance_factory_test",
    size = "small",
    srcs = ["metric_instance_factory_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/metric_instance/src:metric_instance",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
