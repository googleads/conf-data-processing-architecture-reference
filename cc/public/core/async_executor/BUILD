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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "async_executor_factory",
    srcs = [
        "async_executor_factory.cc",
        "async_executor_factory.h",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
    ],
)

cc_test(
    name = "async_executor_factory_test",
    size = "small",
    srcs = ["async_executor_factory_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/public/core/async_executor:async_executor_factory",
        "@com_google_googletest//:gtest_main",
    ],
)
