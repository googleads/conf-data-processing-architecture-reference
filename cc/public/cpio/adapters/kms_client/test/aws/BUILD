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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//cc:scp_cc_internal_pkg"])

cc_test(
    name = "aws_kms_client_test",
    size = "small",
    srcs = ["aws_kms_client_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/auth_token_provider/mock:auth_token_provider_mock",
        "//cc/cpio/client_providers/kms_client_provider/mock:kms_client_provider_mock",
        "//cc/cpio/client_providers/kms_client_provider/src:kms_client_provider_select_lib",
        "//cc/cpio/client_providers/role_credentials_provider/mock:role_credentials_provider_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/interface/kms_client",
        "//cc/public/cpio/interface/kms_client/aws:aws_kms_client",
        "//cc/public/cpio/proto/kms_service/v1:kms_service_cc_proto",
        "//cc/public/cpio/test/global_cpio:test_lib_cpio",
        "@com_google_googletest//:gtest_main",
    ],
)
