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

load(
    "@com_google_googleapis_imports//:imports.bzl",
    "cc_proto_library",
)
load("@rules_java//java:defs.bzl", "java_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "job_lifecycle_helper_proto",
    srcs = [
        "configuration_keys.proto",
        "job_lifecycle_helper.proto",
    ],
    deps = [
        "//cc/core/common/proto:core_common_proto",
        "//cc/public/cpio/proto/job_service/v1:job_service_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

##############################################################################
# C++
##############################################################################
cc_proto_library(
    name = "job_lifecycle_helper_cc_proto",
    deps = [":job_lifecycle_helper_proto"],
)

##############################################################################
# Java
##############################################################################
java_proto_library(
    name = "job_lifecycle_helper_java_proto",
    deps = [":job_lifecycle_helper_proto"],
)
