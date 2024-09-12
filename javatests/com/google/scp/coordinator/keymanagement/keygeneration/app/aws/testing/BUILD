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
load("//testing/aws/build_defs:sam.bzl", "sam_config")

package(default_visibility = ["//visibility:public"])

sam_config(
    name = "key_rotation_generated.yaml",
    deploy_jar_file = "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws:KeyGenerationLambda_deploy.jar",
    sam_config_template = "key_rotation_template.yaml",
)

java_library(
    name = "testing",
    srcs = glob(["*.java"]),
    data = [
        ":key_rotation_generated.yaml",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws:split_key_generation",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/operator/cpio/cryptoclient:aws",
        "//java/external:awaitility",
        "//java/external:aws_kms",
        "//java/external:aws_sqs",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_databind",
        "//java/external:javax_inject",
        "//java/external:jcommander",
        "//java/external:localstack_containers",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/aws:annotations",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)
