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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "PublicKeyApiGatewayHandlerTest",
    srcs = [
        "PublicKeyApiGatewayHandlerTest.java",
    ],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws:PublicKeyApiGatewayHandler",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)

java_test(
    name = "GetEncryptionKeyApiGatewayHandlerTest",
    srcs = [
        "GetEncryptionKeyApiGatewayHandlerTest.java",
    ],
    deps = [
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws:GetEncryptionKeyApiGatewayHandler",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "PublicKeyHostingIntegrationTest",
    size = "large",
    srcs = [
        "PublicKeyHostingIntegrationTest.java",
    ],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:aws_dynamodb",
        "//java/external:aws_lambda_v2",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)

java_test(
    name = "GetEncryptionKeyApiGatewayHandlerIntegrationTest",
    size = "large",
    srcs = [
        "GetEncryptionKeyApiGatewayHandlerIntegrationTest.java",
    ],
    tags = [
        "no-sandbox",
    ],
    deps = [
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/operator/cpio/cryptoclient:aws",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:aws_kms",
        "//java/external:aws_lambda_v2",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:guice-testlib",
        "//java/external:jackson_core",
        "//java/external:localstack_containers",
        "//java/external:mockito",
        "//java/external:protobuf_java_util",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "ListRecentEncryptionKeysApiGatewayHandlerTest",
    srcs = [
        "ListRecentEncryptionKeysApiGatewayHandlerTest.java",
    ],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws:ListRecentEncryptionKeysApiGatewayHandler",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "ListRecentEncryptionKeysApiGatewayHandlerIntegrationTest",
    size = "large",
    srcs = [
        "ListRecentEncryptionKeysApiGatewayHandlerIntegrationTest.java",
    ],
    tags = [
        "no-sandbox",
    ],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:aws_kms",
        "//java/external:aws_lambda_v2",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:guice-testlib",
        "//java/external:jackson_core",
        "//java/external:localstack_containers",
        "//java/external:mockito",
        "//java/external:protobuf_java_util",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/testing",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/shared/testutils/aws",
        "//shared/protos/api/v1:java_proto",
    ],
)
