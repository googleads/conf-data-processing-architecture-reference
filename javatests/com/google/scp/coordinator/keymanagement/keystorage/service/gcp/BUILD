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
    name = "CreateKeyRequestHandlerTest",
    srcs = [
        "CreateKeyRequestHandlerTest.java",
    ],
    deps = [
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/service/gcp:KeyStorageServiceHttpFunction",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/gcp",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:gcp_cloud_function",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:tink",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)

java_test(
    name = "KeyStorageServiceHttpFunctionTest",
    srcs = [
        "KeyStorageServiceHttpFunctionTest.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/service/gcp:KeyStorageServiceHttpFunction",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:gcp_cloud_function",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)

java_test(
    name = "KeyStorageServiceIntegrationTest",
    srcs = [
        "KeyStorageServiceIntegrationTest.java",
    ],
    deps = [
        ":GcpKeyStorageServiceTestEnvModule",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/gcp",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/util",
        "//java/external:acai",
        "//java/external:gcp_spanner",
        "//java/external:google_truth",
        "//java/external:gson",
        "//java/external:guice",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//javatests/com/google/kms:LocalGcpKmsClient",
        "//javatests/com/google/kms:LocalGcpKmsServer",
        "//javatests/com/google/kms:LocalKmsServerContainer",
        "//javatests/com/google/scp/coordinator/keymanagement/keystorage/service/gcp/testing:LocalKeyStorageServiceHttpFunction",
        "//javatests/com/google/scp/coordinator/keymanagement/shared/dao/gcp",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/gcp",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/gcp/cloudfunction",
        "//javatests/com/google/scp/shared/testutils/common",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)

java_library(
    name = "GcpKeyStorageServiceTestEnvModule",
    srcs = ["GcpKeyStorageServiceTestEnvModule.java"],
    deps = [
        "//java/com/google/scp/shared/util",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//javatests/com/google/kms:LocalGcpKmsClient",
        "//javatests/com/google/kms:LocalKmsServerContainer",
        "//javatests/com/google/scp/coordinator/keymanagement/keystorage/service/gcp/testing:KeyStorageHttpFunctionModule",
        "//javatests/com/google/scp/coordinator/keymanagement/keystorage/service/gcp/testing:LocalKeyStorageServiceHttpFunction",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/gcp",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)
