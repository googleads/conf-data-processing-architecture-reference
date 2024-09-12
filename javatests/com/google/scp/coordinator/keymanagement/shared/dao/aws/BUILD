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

load("@rules_java//java:defs.bzl", "java_library", "java_test")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "DynamoKeyDbTestEnv",
    srcs = ["DynamoKeyDbTestEnv.java"],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/external:acai",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_regions",
        "//java/external:aws_url_connection_client",
        "//java/external:commons_logging",
        "//java/external:guice",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)

java_test(
    name = "DynamoKeyDbTest",
    srcs = [
        "DynamoKeyDbTest.java",
    ],
    deps = [
        ":DynamoKeyDbTestEnv",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:google_truth",
        "//java/external:guice",
        "//javatests/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)
