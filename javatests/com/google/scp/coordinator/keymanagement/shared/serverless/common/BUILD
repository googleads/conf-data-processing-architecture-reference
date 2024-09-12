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

load("@rules_java//java:defs.bzl", "java_test")
load("@rules_proto//proto:defs.bzl", "proto_library")

java_test(
    name = "ApiTaskTest",
    srcs = [
        "ApiTaskTest.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/shared/serverless/common",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:mockito",
    ],
)

java_test(
    name = "ResponseContextTest",
    srcs = [
        "ResponseContextTest.java",
    ],
    deps = [
        ":common_java_proto",
        "//java/com/google/scp/coordinator/keymanagement/shared/serverless/common",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:mockito",
    ],
)

java_test(
    name = "ServerlessFunctionTest",
    srcs = [
        "ServerlessFunctionTest.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/shared/serverless/common",
        "//java/com/google/scp/shared/api/model",
        "//java/external:guice",
        "//java/external:mockito",
    ],
)

proto_library(
    name = "common_proto",
    srcs = ["common.proto"],
)

java_proto_library(
    name = "common_java_proto",
    deps = [":common_proto"],
)
