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
    name = "AwsCreateSplitKeyTaskTestBase",
    srcs = ["AwsCreateSplitKeyTaskTestBase.java"],
    deps = [
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/aws",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyid",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/coordinator/keymanagement/shared/util",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/util",
        "//java/external:acai",
        "//java/external:aws_core",
        "//java/external:aws_kms",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
        "//java/external:junit",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/app/common/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)

java_test(
    name = "AwsCreateSplitKeyTaskTest",
    srcs = [
        "AwsCreateSplitKeyTaskTest.java",
    ],
    deps = [
        "AwsCreateSplitKeyTaskTestBase",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/aws",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyid",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/coordinator/keymanagement/shared/util",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/external:acai",
        "//java/external:aws_core",
        "//java/external:aws_kms",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/app/common/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)

java_test(
    name = "AwsSeqIdCreateSplitKeyTaskTest",
    srcs = [
        "AwsSeqIdCreateSplitKeyTaskTest.java",
    ],
    deps = [
        "AwsCreateSplitKeyTaskTestBase",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/aws",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyid",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/coordinator/keymanagement/shared/util",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/external:acai",
        "//java/external:aws_core",
        "//java/external:aws_kms",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/app/common/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)
