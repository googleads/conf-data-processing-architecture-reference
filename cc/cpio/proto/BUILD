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

load(
    "@com_google_googleapis_imports//:imports.bzl",
    "cc_proto_library",
)
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "instance_database_client_proto",
    srcs = ["instance_database_client.proto"],
    deps = [
        "//operator/protos/shared/backend/asginstance:proto",
    ],
)

##############################################################################
# C++
##############################################################################
cc_proto_library(
    name = "instance_database_client_cc_proto",
    deps = [":instance_database_client_proto"],
)
