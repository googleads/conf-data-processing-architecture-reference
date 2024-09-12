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

# Separate target for injection tokens to prevent cyclical dependencies.
java_library(
    name = "annotations",
    srcs = [
        "Annotations.java",
    ],
    deps = [
        "//java/external:guice",
    ],
)

java_library(
    name = "aws",
    srcs = glob(["*.java"]),
    deps = [
        ":annotations",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/external:acai",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_sqs",
        "//java/external:guava",
        "//java/external:guice",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)
