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

load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "testutils",
    srcs = glob(["*.java"]),
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/keystorage/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/coordinator/keymanagement/shared/util",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/proto",
        "//java/com/google/scp/shared/util",
        "//java/external:acai",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_lambda_v2",
        "//java/external:aws_regions",
        "//java/external:aws_s3",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//java/external:protobuf_java",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/testing",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)
