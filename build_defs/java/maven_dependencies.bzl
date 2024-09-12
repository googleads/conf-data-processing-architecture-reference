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

load("@rules_jvm_external//:defs.bzl", "maven_install")
load("//build_defs/shared:java_grpc.bzl", "GAPIC_GENERATOR_JAVA_VERSION")
load("//build_defs/tink:tink_defs.bzl", "TINK_MAVEN_ARTIFACTS")

JACKSON_VERSION = "2.15.2"

AUTO_VALUE_VERSION = "1.7.4"

AWS_SDK_VERSION = "2.17.239"

GOOGLE_GAX_VERSION = "2.4.0"

AUTO_SERVICE_VERSION = "1.0"

def maven_dependencies():
    maven_install(
        name = "maven",
        artifacts = [
            "com.amazonaws:aws-lambda-java-core:1.2.1",
            "com.amazonaws:aws-lambda-java-events:3.8.0",
            "com.amazonaws:aws-lambda-java-events-sdk-transformer:3.1.0",
            "com.amazonaws:aws-java-sdk-sqs:1.11.860",
            "com.amazonaws:aws-java-sdk-s3:1.11.860",
            "com.amazonaws:aws-java-sdk-kms:1.11.860",
            "com.amazonaws:aws-java-sdk-core:1.11.860",
            "com.beust:jcommander:1.81",
            "com.fasterxml.jackson.core:jackson-annotations:" + JACKSON_VERSION,
            "com.fasterxml.jackson.core:jackson-core:" + JACKSON_VERSION,
            "com.fasterxml.jackson.core:jackson-databind:" + JACKSON_VERSION,
            "com.fasterxml.jackson.datatype:jackson-datatype-guava:" + JACKSON_VERSION,
            "com.fasterxml.jackson.datatype:jackson-datatype-jsr310:" + JACKSON_VERSION,
            "com.fasterxml.jackson.datatype:jackson-datatype-jdk8:" + JACKSON_VERSION,
            "com.google.acai:acai:1.1",
            "com.google.auto.factory:auto-factory:1.0",
            "com.google.auto.service:auto-service-annotations:" + AUTO_SERVICE_VERSION,
            "com.google.auto.service:auto-service:" + AUTO_SERVICE_VERSION,
            "com.google.auto.value:auto-value-annotations:" + AUTO_VALUE_VERSION,
            "com.google.auto.value:auto-value:" + AUTO_VALUE_VERSION,
            "com.google.code.findbugs:jsr305:3.0.2",
            "com.google.code.gson:gson:2.8.9",
            "com.google.cloud:google-cloud-kms:2.1.2",
            "com.google.cloud:google-cloud-pubsub:1.114.4",
            "com.google.cloud:google-cloud-storage:1.118.0",
            "com.google.cloud:google-cloud-storage-transfer:1.17.0",
            "com.google.cloud:google-cloud-spanner:6.12.2",
            "com.google.cloud:google-cloud-secretmanager:2.2.0",
            "com.google.cloud:google-cloud-compute:1.12.1",
            "com.google.cloud:google-cloudevent-types:0.3.0",
            "com.google.api.grpc:proto-google-cloud-compute-v1:1.12.1",
            "com.google.cloud.functions.invoker:java-function-invoker:1.3.1",
            "com.google.auth:google-auth-library-oauth2-http:1.11.0",
            "com.google.cloud.functions:functions-framework-api:1.0.4",
            "commons-logging:commons-logging:1.1.1",
            "com.google.api:gax:" + GOOGLE_GAX_VERSION,
            "com.google.http-client:google-http-client-jackson2:1.40.0",
            "com.google.cloud:google-cloud-monitoring:3.4.1",
            "com.google.api.grpc:proto-google-cloud-monitoring-v3:3.4.1",
            "com.google.api.grpc:proto-google-cloud-storage-transfer-v1:1.17.0",
            "com.google.api.grpc:proto-google-common-protos:2.27.0",
            "com.google.guava:guava:33.2.1-jre",
            "com.google.guava:guava-testlib:33.2.1-jre",
            "com.google.inject:guice:5.1.0",
            "com.google.inject.extensions:guice-assistedinject:5.1.0",
            "com.google.inject.extensions:guice-testlib:5.1.0",
            "com.google.jimfs:jimfs:1.2",
            "com.google.testparameterinjector:test-parameter-injector:1.1",
            "com.google.truth.extensions:truth-java8-extension:1.3.0",
            "com.google.truth.extensions:truth-proto-extension:1.3.0",
            "com.google.truth:truth:1.3.0",
            "com.jayway.jsonpath:json-path:2.5.0",
            "io.cloudevents:cloudevents-api:2.5.0",
            "io.github.resilience4j:resilience4j-core:1.7.1",
            "io.github.resilience4j:resilience4j-retry:1.7.1",
            "javax.annotation:javax.annotation-api:1.3.2",
            "javax.inject:javax.inject:1",
            "junit:junit:4.12",
            "org.apache.avro:avro:1.10.2",
            "org.apache.commons:commons-math3:3.6.1",
            "org.apache.httpcomponents:httpcore:4.4.14",
            "org.apache.httpcomponents:httpclient:4.5.13",
            "org.apache.httpcomponents.client5:httpclient5:5.1.3",
            "org.apache.httpcomponents.core5:httpcore5:5.1.4",
            "org.apache.httpcomponents.core5:httpcore5-h2:5.1.4",  # Explicit transitive dependency to avoid https://issues.apache.org/jira/browse/HTTPCLIENT-2222
            "org.apache.logging.log4j:log4j-1.2-api:2.17.0",
            "org.apache.logging.log4j:log4j-core:2.17.0",
            "org.awaitility:awaitility:3.0.0",
            "org.mock-server:mockserver-core:5.11.2",
            "org.mock-server:mockserver-junit-rule:5.11.2",
            "org.mock-server:mockserver-client-java:5.11.2",
            "org.hamcrest:hamcrest-library:1.3",
            "org.mockito:mockito-core:3.12.4",
            "org.mockito:mockito-inline:3.12.4",
            "org.slf4j:slf4j-api:1.7.30",
            "org.slf4j:slf4j-simple:1.7.30",
            "org.slf4j:slf4j-log4j12:1.7.30",
            "org.testcontainers:testcontainers:1.15.3",
            "org.testcontainers:localstack:1.15.3",
            "software.amazon.awssdk:apigateway:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:arns:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:autoscaling:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:aws-sdk-java:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:dynamodb-enhanced:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:dynamodb:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:cloudwatch:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:ec2:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:pricing:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:regions:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:s3:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:aws-core:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:ssm:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:sts:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:sqs:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:url-connection-client:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:utils:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:auth:" + AWS_SDK_VERSION,
            "software.amazon.awssdk:lambda:" + AWS_SDK_VERSION,
            "com.google.api:gapic-generator-java:" + GAPIC_GENERATOR_JAVA_VERSION,  # To use generated gRpc Java interface
            "io.grpc:grpc-netty:1.54.0",
        ] + TINK_MAVEN_ARTIFACTS,
        repositories = [
            "https://repo1.maven.org/maven2",
        ],
    )

    maven_install(
        name = "maven_yaml",
        artifacts = [
            "org.yaml:snakeyaml:1.27",
        ],
        repositories = [
            "https://repo1.maven.org/maven2",
        ],
        # Pin the working version for snakeyaml.
        version_conflict_policy = "pinned",
    )
