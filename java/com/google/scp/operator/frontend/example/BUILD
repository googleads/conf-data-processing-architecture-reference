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

package(default_visibility = ["//visibility:public"])

java_library(
    name = "GcpCmrtFrontendServiceTest",
    srcs = ["GcpCmrtFrontendServiceTest.java"],
    deps = [
        "//cc/public/cpio/proto/cmrtworker/v1:job_data_java_proto",
        "//cc/public/cpio/proto/job_service/v1:job_service_java_proto",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:google_truth",
        "//java/external:jackson_databind",
        "@com_google_protobuf//java/util",
    ],
)
