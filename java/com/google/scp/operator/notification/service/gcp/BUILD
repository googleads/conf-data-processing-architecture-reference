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

load("@rules_java//java:defs.bzl", "java_binary", "java_library")
load("@rules_pkg//:pkg.bzl", "pkg_zip")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "JobNotificationEventHandler",
    srcs = glob([
        "*.java",
    ]),
    deps = [
        "//java/com/google/scp/operator/cpio/notificationclient:gcp",
        "//java/com/google/scp/operator/cpio/notificationclient:model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/gcp/util",
        "//java/com/google/scp/shared/gcp/util:cloud_event_util",
        "//java/com/google/scp/shared/gcp/util:json_util",
        "//java/external:gcp_cloud_function",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:io_cloudevents_api",
        "//java/external:jackson_databind",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/jobnotification:job_notification_event_java_proto",
        "@com_google_protobuf//java/core",
    ],
)

java_binary(
    name = "JobNotificationCloudFunction",
    runtime_deps = [
        ":JobNotificationEventHandler",
    ],
)

alias(
    name = "JobNotificationCloudFunctionDeploy",
    actual = ":JobNotificationCloudFunction_deploy.jar",
)

pkg_zip(
    name = "JobNotificationCloudFunctionDeploy_zip",
    srcs = [
        ":JobNotificationCloudFunction_deploy.jar",
    ],
)
