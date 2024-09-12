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

load("@rules_java//java:defs.bzl", "java_binary", "java_test")

coordinator_keymanagement_test = [
    "//javatests/com/google/scp/coordinator/keymanagement:__subpackages__",
]

package(default_visibility = coordinator_keymanagement_test)

java_binary(
    name = "LocalGcpKmsServer",
    srcs = [
        "LocalGcpKmsServer.java",
        "LocalKmsConstants.java",
    ],
    deps = [
        "//java/external:google_api_services_cloudkms",
        "//java/external:gson",
        "//java/external:guava",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
    ],
)

alias(
    name = "LocalGcpKmsServerDeploy",
    actual = ":LocalGcpKmsServer_deploy.jar",
)

java_binary(
    name = "LocalKmsServerContainer",
    srcs = ["LocalKmsServerContainer.java"],
    deps = ["//java/external:testcontainers"],
)

java_binary(
    name = "LocalGcpKmsClient",
    srcs = [
        "LocalGcpKmsClient.java",
    ],
    deps = [
        ":LocalKmsServerContainer",
        "//java/external:gson",
        "//java/external:testcontainers",
        "//javatests/com/google/kms:LocalGcpKmsServerDeploy",
        "//javatests/com/google/scp/shared/testutils/common",
    ],
)

java_test(
    name = "LocalKmsServerContainerTest",
    srcs = [
        "LocalKmsServerContainerTest.java",
    ],
    deps = [
        ":LocalGcpKmsClient",
        ":LocalKmsServerContainer",
        "//java/external:google_truth",
        "//java/external:gson",
        "//java/external:testcontainers",
        "//javatests/com/google/kms:LocalGcpKmsServerDeploy",
        "//javatests/com/google/scp/shared/testutils/common",
    ],
)
