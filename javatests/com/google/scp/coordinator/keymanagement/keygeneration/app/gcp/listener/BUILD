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

load("@rules_java//java:defs.bzl", "java_library", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "CreateKeysPubSubListenerTest",
    srcs = ["CreateKeysPubSubListenerTest.java"],
    deps = [
        ":gcp",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/gcp/listener",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:gcp_pubsub",
        "//java/external:gcp_pubsub_v1",
        "//java/external:google_truth",
        "//java/external:grpc_api",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)

java_test(
    name = "CreateSplitKeysPubSubListenerTest",
    srcs = ["CreateSplitKeysPubSubListenerTest.java"],
    deps = [
        ":gcp",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/gcp/listener",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:gcp_pubsub",
        "//java/external:gcp_pubsub_v1",
        "//java/external:google_truth",
        "//java/external:grpc_api",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:mockito_inline",
        "//java/external:protobuf_java",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)

java_library(
    name = "gcp",
    testonly = True,
    srcs = ["PubSubKeyGenerationTestModule.java"],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/gcp:key_generation",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:gcp_api_gax",
        "//java/external:gcp_gax_grpc",
        "//java/external:gcp_pubsub",
        "//java/external:gcp_pubsub_v1",
        "//java/external:google_truth",
        "//java/external:grpc_api",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)
