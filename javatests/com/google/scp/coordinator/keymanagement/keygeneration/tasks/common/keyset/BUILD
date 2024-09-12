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

java_test(
    name = "KeySetConfigTest",
    srcs = [
        "KeySetConfigTest.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyset",
        "//java/external:google_truth",
    ],
)

java_test(
    name = "KeySetManagerTest",
    srcs = [
        "KeySetManagerTest.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyset",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/shared/util",
        "//java/external:google_truth",
        "//java/external:guice",
    ],
)

java_test(
    name = "KeySetsConfigTest",
    srcs = [
        "KeySetsConfigTest.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyset",
        "//java/external:google_truth",
        "//java/external:jackson_databind",
    ],
)
