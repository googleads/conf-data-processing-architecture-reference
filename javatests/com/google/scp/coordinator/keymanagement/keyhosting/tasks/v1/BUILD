# Copyright 2024 Google LLC
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
    name = "GetActivePublicKeysTaskTest",
    srcs = [
        "GetActivePublicKeysTaskTest.java",
    ],
    # TODO(b/358102551) Works around non-hermetic sandbox issue until upgrade
    # to Bazel 7+ or incompatible_sandbox_hermetic_tmp can be reliably applied.
    tags = ["no-sandbox"],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks/v1",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/serverless/common",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:mockito_inline",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)

java_test(
    name = "ListRecentEncryptionKeysTaskTest",
    srcs = [
        "ListRecentEncryptionKeysTaskTest.java",
    ],
    # TODO(b/358102551) Works around non-hermetic sandbox issue until upgrade
    # to Bazel 7+ or incompatible_sandbox_hermetic_tmp can be reliably applied.
    tags = ["no-sandbox"],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/backend:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks/v1",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/testing",
        "//java/com/google/scp/coordinator/keymanagement/shared/serverless/common",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:mockito_inline",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
    ],
)
