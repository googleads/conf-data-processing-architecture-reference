/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keyhosting.service.aws.testing;

import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.addLambdaHandler;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.getLocalStackHostname;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.getRegionName;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.uploadLambdaCode;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.BindingAnnotation;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAEncryptionKeyServiceBaseUrl;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.nio.file.Path;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.services.apigateway.ApiGatewayClient;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.s3.S3Client;

/**
 * Creates a local version of GetEncryptionKeyHostingService using LocalStack. The service is not
 * deployed until the provided {@link CoordinatorAEncryptionKeyServiceBaseUrl} is injected so it
 * should have minimal test impact if unused.
 */
public final class LocalGetEncryptionKeyHostingServiceModule extends AbstractModule {
  // LINT.IfChange(encryption-deploy-jar)
  private static final Path jarPath =
      Path.of(
          "java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/EncryptionKeyServiceLambda_deploy.jar");
  // LINT.ThenChange(BUILD:encryption-deploy-jar)
  private static final String LAMBDA_JAR_KEY = "app/private_key_v1_lambda.jar";
  private static final String HANDLER_CLASS =
      "com.google.scp.coordinator.keymanagement.keyhosting.service.aws.GetEncryptionKeyApiGatewayHandler";

  @Provides
  @Singleton
  @EncryptionKeyLambdaArn
  public String provideLambdaArn(
      LocalStackContainer localStack,
      LambdaClient lambdaClient,
      S3Client s3Client,
      @DynamoKeyDbTableName String tableName) {
    uploadLambdaCode(s3Client, LAMBDA_JAR_KEY, jarPath);

    return addLambdaHandler(
        lambdaClient,
        "integration_get_private_key_lambda",
        HANDLER_CLASS,
        LAMBDA_JAR_KEY,
        ImmutableMap.of(
            "KEYSTORE_TABLE_NAME",
            tableName,
            "AWS_REGION",
            getRegionName(),
            "KEYSTORE_ENDPOINT_OVERRIDE",
            String.format("http://%s:4566", getLocalStackHostname(localStack))));
  }

  @Provides
  @Singleton
  @CoordinatorAEncryptionKeyServiceBaseUrl
  public String provideLambdaBaseUrl(
      @EncryptionKeyLambdaArn String lambdaArn,
      ApiGatewayClient client,
      LocalStackContainer localStack) {
    return AwsIntegrationTestUtil.createApiGatewayHandler(localStack, client, lambdaArn);
  }

  /** ARN of the deployed lambda, used internally to ensure the lambda is lazily loaded. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  private @interface EncryptionKeyLambdaArn {}
}
