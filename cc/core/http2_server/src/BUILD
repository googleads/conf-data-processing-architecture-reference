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

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "core_http2_server_lib",
    srcs = glob(
        [
            "**/*.cc",
            "**/*.h",
        ],
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/authorization_proxy/src:core_authorization_proxy_lib",
        "//cc/core/common/time_provider/src:time_provider_lib",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/public/cpio/interface/metric_client:metric_client_interface",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/metric_instance/interface:metric_instance_interface",
        "//cc/public/cpio/utils/metric_instance/interface:type_def",
        "//cc/public/cpio/utils/metric_instance/src:metric_instance",
        "@boost//:asio_ssl",
        "@boost//:date_time",
        "@boost//:exception",
        "@boost//:system",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@com_google_absl//absl/strings",
    ],
)
