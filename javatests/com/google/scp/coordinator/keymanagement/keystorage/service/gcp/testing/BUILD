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

load("@rules_java//java:defs.bzl", "java_binary", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "KeyStorageHttpFunctionModule",
    testonly = True,
    srcs = [
        "KeyStorageHttpFunctionModule.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/gcp",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/gcp",
        "//java/com/google/scp/shared/util",
        "//java/external:guice",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//javatests/com/google/kms:LocalGcpKmsClient",
        "//javatests/com/google/kms:LocalGcpKmsServer",
        "//javatests/com/google/kms:LocalKmsServerContainer",
        "//javatests/com/google/scp/coordinator/keymanagement/shared/dao/gcp",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/gcp:spanner_key_db_test_util",
    ],
)

java_library(
    name = "LocalKeyStorageServiceHttpFunction",
    testonly = True,
    srcs = [
        "LocalKeyStorageServiceHttpFunction.java",
    ],
    deps = [
        ":KeyStorageHttpFunctionModule",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/service/gcp:KeyStorageServiceHttpFunction",
        "//java/external:guice",
        "//java/external:tink",
    ],
)

java_binary(
    name = "LocalKeyStorageServiceHttpCloudFunction",
    testonly = True,
    runtime_deps = [
        ":LocalKeyStorageServiceHttpFunction",
    ],
)

alias(
    name = "LocalKeyStorageServiceHttpCloudFunctionDeploy",
    testonly = True,
    actual = ":LocalKeyStorageServiceHttpCloudFunction_deploy.jar",
)
