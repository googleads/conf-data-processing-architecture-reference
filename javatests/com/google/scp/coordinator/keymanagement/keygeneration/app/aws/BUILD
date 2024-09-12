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
    name = "SplitKeyGenerationIntegrationTestBase",
    srcs = ["SplitKeyGenerationIntegrationTestBase.java"],
    deps = [
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyid",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/external:acai",
        "//java/external:apache_httpclient",
        "//java/external:awaitility",
        "//java/external:aws_core",
        "//java/external:aws_kms",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_sqs",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
        "//java/external:junit",
        "//java/external:localstack_containers",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/app/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/keystorage/service/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/aws",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)

java_test(
    name = "KeyGenerationLambdaIntegrationTest",
    size = "large",
    timeout = "long",
    srcs = [
        "KeyGenerationLambdaIntegrationTest.java",
    ],
    tags = [
        "no-sandbox",
        # TODO(b/261876761): Allow other tests to run in parallel,
        # remove once tests no longer interfere with one another
        "exclusive",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/shared/util",
        "//java/external:acai",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_dynamodb",
        "//java/external:aws_kms",
        "//java/external:aws_regions",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/app/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)

java_test(
    name = "KeyGenerationLambdaTest",
    srcs = [
        "KeyGenerationLambdaTest.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws:keygeneration",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:mockito",
    ],
)

java_test(
    name = "SqsKeyGenerationQueueTest",
    srcs = [
        "SqsKeyGenerationQueueTest.java",
        "SqsKeyGenerationQueueTestEnv.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws/model",
        "//java/external:acai",
        "//java/external:aws_core",
        "//java/external:aws_sqs",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/aws",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)

java_test(
    name = "SplitKeyGenerationIntegrationTest",
    size = "large",
    # Takes over 4 minutes on a dev machine, play it safe and increase timeout to 15 minutes.
    timeout = "long",
    srcs = [
        "SplitKeyGenerationIntegrationTest.java",
    ],
    tags = [
        # TODO(b/261876761): Allow other tests to run in parallel,
        # remove once tests no longer interfere with one another
        "exclusive",
    ],
    deps = [
        "SplitKeyGenerationIntegrationTestBase",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/external:acai",
        "//java/external:apache_httpclient",
        "//java/external:awaitility",
        "//java/external:aws_core",
        "//java/external:aws_kms",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_sqs",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/app/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/keystorage/service/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/aws",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)

java_test(
    name = "SequenceIdSplitKeyGenerationIntegrationTest",
    size = "large",
    # Takes over 4 minutes on a dev machine, play it safe and increase timeout to 15 minutes.
    timeout = "long",
    srcs = [
        "SequenceIdSplitKeyGenerationIntegrationTest.java",
    ],
    tags = [
        # TODO(b/261876761): Allow other tests to run in parallel,
        # remove once tests no longer interfere with one another
        "exclusive",
    ],
    deps = [
        "SplitKeyGenerationIntegrationTestBase",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyid",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/external:acai",
        "//java/external:apache_httpclient",
        "//java/external:awaitility",
        "//java/external:aws_core",
        "//java/external:aws_kms",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_sqs",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
        "//javatests/com/google/scp/coordinator/keymanagement/keygeneration/app/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/keystorage/service/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/aws",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)
