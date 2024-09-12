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

load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "testing",
    srcs = glob(["*.java"]),
    data = [
        # LINT.IfChange(public-deploy-jar)
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws:PublicKeyApiGatewayHandlerLambda_deploy.jar",
        # LINT.ThenChange(LocalPublicKeyHostingServiceModule.java:public-deploy-jar)
        # LINT.IfChange(encryption-deploy-jar)
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws:EncryptionKeyServiceLambda_deploy.jar",
        # LINT.ThenChange(LocalPrivateKeyHostingServiceV2Module.java:encryption-deploy-jar)
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/operator/cpio/cryptoclient",
        "//java/com/google/scp/operator/cpio/cryptoclient:aws",
        "//java/external:acai",
        "//java/external:aws_api_gateway_v1",
        "//java/external:aws_lambda_v2",
        "//java/external:aws_s3",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_databind",
        "//java/external:localstack_containers",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/aws",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils/aws:annotations",
        "//javatests/com/google/scp/shared/testutils/aws",
        "//javatests/com/google/scp/shared/testutils/common",
    ],
)
