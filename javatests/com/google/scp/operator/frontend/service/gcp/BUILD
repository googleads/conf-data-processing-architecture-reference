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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "CreateJobRequestHandlerTest",
    srcs = [
        "CreateJobRequestHandlerTest.java",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/shared/api/exception",
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
    name = "GetJobRequestHandlerTest",
    srcs = [
        "GetJobRequestHandlerTest.java",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/mapper",
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
    name = "PutJobRequestHandlerTest",
    srcs = [
        "PutJobRequestHandlerTest.java",
    ],
    deps = [
        "//cc/public/cpio/proto/job_service/v1:job_service_java_proto",
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:hello_world_java_proto",
        "//java/com/google/scp/shared/api/exception",
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
        "@com_google_protobuf//:protobuf_java",
    ],
)

java_test(
    name = "GetJobByIdRequestHandlerTest",
    srcs = [
        "GetJobByIdRequestHandlerTest.java",
    ],
    deps = [
        "//cc/public/cpio/proto/job_service/v1:job_service_java_proto",
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:gcp_cloud_function",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:mockito",
        "//java/external:protobuf_java_util",
    ],
)
