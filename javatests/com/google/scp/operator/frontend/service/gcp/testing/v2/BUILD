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
    name = "FrontendServiceV2Module",
    srcs = glob([
        "FrontendServiceV2Module.java",
    ]),
    deps = [
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/external:guice",
    ],
)

java_library(
    name = "LocalFrontendServiceV2HttpFunction",
    testonly = True,
    srcs = [
        "LocalFrontendServiceV2HttpFunction.java",
    ],
    deps = [
        ":FrontendServiceV2Module",
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/external:gcp_core",
        "//java/external:gcp_spanner",
        "//java/external:guice",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/operator/frontend/service/gcp/testing:FrontendServiceHttpFunctionTestModule",
    ],
)

java_binary(
    name = "LocalFrontendServiceV2HttpCloudFunction",
    testonly = True,
    runtime_deps = [
        ":LocalFrontendServiceV2HttpFunction",
    ],
)

alias(
    name = "LocalFrontendServiceV2HttpCloudFunctionDeploy",
    testonly = True,
    actual = ":LocalFrontendServiceV2HttpCloudFunction_deploy.jar",
)

java_library(
    name = "FrontendServiceV2CloudFunctionEmulatorContainer",
    srcs = ["FrontendServiceV2CloudFunctionEmulatorContainer.java"],
    runtime_deps = [
        ":LocalFrontendServiceV2HttpCloudFunctionDeploy",
        "//java/external:gcp_function_invoker",
    ],
    deps = [
        "//java/external:guice",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)

java_library(
    name = "FrontendServiceV2IntegrationTestEnv",
    srcs = glob([
        "FrontendServiceV2IntegrationTestEnv.java",
    ]),
    deps = [
        ":FrontendServiceV2CloudFunctionEmulatorContainer",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/external:gcp_spanner",
        "//java/external:guice",
        "//javatests/com/google/scp/operator/frontend/service/gcp/testing:FrontendServiceIntegrationTestModule",
        "//javatests/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)

java_test(
    name = "FrontendServiceV2HttpFunctionTest",
    srcs = [
        "FrontendServiceV2HttpFunctionTest.java",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:gcp_cloud_function",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:mockito",
        "//java/external:protobuf_java_util",
        "//operator/protos/frontend/api/v1:java_proto",
    ],
)

java_test(
    name = "GcpFrontendServiceV2IntegrationTest",
    srcs = [
        "GcpFrontendServiceV2IntegrationTest.java",
    ],
    deps = [
        ":FrontendServiceV2IntegrationTestEnv",
        "//cc/public/cpio/proto/job_service/v1:job_service_java_proto",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:hello_world_java_proto",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:gcp_api_gax",
        "//java/external:gcp_cloud_function",
        "//java/external:gcp_core",
        "//java/external:gcp_gax_grpc",
        "//java/external:gcp_pubsub",
        "//java/external:gcp_pubsub_v1",
        "//java/external:gcp_spanner",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:grpc_api",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:guice-testlib",
        "//java/external:jackson_annotations",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:javax_inject",
        "//java/external:localstack_containers",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
        "//javatests/com/google/scp/shared/testutils/common",
        "//javatests/com/google/scp/shared/testutils/gcp",
        "//operator/protos/frontend/api/v1:java_proto",
        "//operator/protos/shared/backend/jobqueue:java_proto",
    ],
)
