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
    name = "test_gcp_job_client_options",
    hdrs = [
        "test_gcp_job_client_options.h",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/public/cpio/interface/job_client:type_def",
    ],
)

cc_library(
    name = "test_gcp_job_client",
    srcs = [
        "//cc/public/cpio/adapters/job_client/test:test_gcp_job_client_srcs",
    ],
    deps = [
        ":test_gcp_job_client_options",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/common/test/gcp:test_gcp_database_factory_lib",
        "//cc/cpio/client_providers/nosql_database_client_provider/test/gcp:test_gcp_nosql_database_client_provider_lib",
        "//cc/cpio/client_providers/queue_client_provider/test/gcp:test_gcp_queue_client_provider_lib",
        "//cc/cpio/common/src:cpio_utils_lib",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/interface/job_client",
        "//cc/public/cpio/interface/job_client:type_def",
        "//cc/public/cpio/test/nosql_database_client:test_gcp_nosql_database_client_options",
    ],
)
