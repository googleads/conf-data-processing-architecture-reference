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

load("@rules_java//java:defs.bzl", "java_library", "java_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "fake_metadata_db",
    srcs = ["FakeMetadataDb.java"],
    deps = [
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_library(
    name = "fake_asg_instances_dao",
    srcs = ["FakeAsgInstancesDao.java"],
    deps = [
        "//java/com/google/scp/operator/shared/dao/asginstancesdb/common",
        "//operator/protos/shared/backend/asginstance:java_proto",
    ],
)

java_library(
    name = "fake_job_db",
    srcs = ["FakeJobDb.java"],
    deps = [
        "//cc/public/cpio/proto/job_service/v1:job_service_java_proto",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
    ],
)

java_library(
    name = "job_generator",
    srcs = ["JobGenerator.java"],
    deps = [
        ":hello_world_java_proto",
        "//cc/public/cpio/proto/job_service/v1:job_service_java_proto",
        "//java/com/google/scp/shared/proto",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

proto_library(
    name = "hello_world_proto",
    srcs = ["hello_world.proto"],
)

java_proto_library(
    name = "hello_world_java_proto",
    deps = [":hello_world_proto"],
)
