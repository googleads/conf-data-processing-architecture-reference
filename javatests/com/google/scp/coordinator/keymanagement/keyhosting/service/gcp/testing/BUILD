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
    name = "LocalPublicKeyHttpFunction",
    testonly = True,
    srcs = [
        "LocalPublicKeyHttpFunction.java",
    ],
    deps = [
        ":KeyHostingHttpFunctionModule",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp:PublicKeyServiceHttpFunction",
        "//java/external:guice",
    ],
)

java_binary(
    name = "LocalPublicKeyHttpCloudFunction",
    testonly = True,
    runtime_deps = [
        ":LocalPublicKeyHttpFunction",
    ],
)

alias(
    name = "LocalPublicKeyHttpCloudFunctionDeploy",
    testonly = True,
    actual = ":LocalPublicKeyHttpCloudFunction_deploy.jar",
)

java_library(
    name = "KeyHostingHttpFunctionModule",
    testonly = True,
    srcs = [
        "KeyHostingHttpFunctionModule.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/gcp",
        "//java/external:guice",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/gcp:spanner_key_db_test_util",
    ],
)

java_library(
    name = "LocalEncryptionKeyServiceHttpFunction",
    testonly = True,
    srcs = [
        "LocalEncryptionKeyServiceHttpFunction.java",
    ],
    deps = [
        ":KeyHostingHttpFunctionModule",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp:EncryptionKeyServiceHttpFunction",
        "//java/external:guice",
    ],
)

java_binary(
    name = "LocalEncryptionKeyServiceHttpCloudFunction",
    testonly = True,
    runtime_deps = [
        ":LocalEncryptionKeyServiceHttpFunction",
    ],
)

alias(
    name = "LocalEncryptionKeyServiceHttpCloudFunctionDeploy",
    testonly = True,
    actual = ":LocalEncryptionKeyServiceHttpCloudFunction_deploy.jar",
)
