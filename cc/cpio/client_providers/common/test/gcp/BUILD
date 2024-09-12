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

package(default_visibility = ["//cc:scp_cc_internal_pkg"])

cc_library(
    name = "test_gcp_database_factory_lib",
    srcs = [
        "test_gcp_database_factory.cc",
        "test_gcp_database_factory.h",
    ],
    deps = [
        "//cc/cpio/client_providers/common/src/gcp:gcp_database_factory_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/public/cpio/test/common:test_gcp_database_client_options",
    ],
)
