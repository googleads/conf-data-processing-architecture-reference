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

import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.ListPublicKeysBaseUrl;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.addLambdaHandler;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.getLocalStackHostname;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.getRegionName;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.uploadLambdaCode;

import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.BindingAnnotation;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import com.google.scp.shared.testutils.common.RepoUtil;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.services.apigateway.ApiGatewayClient;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.s3.S3Client;

/**
 * Uses the Lambda API to deploy a local version of PublicKeyHostingService which can be invoked by
 * name (injected with: {@link PublicKeyLambdaArn}) using the Lambda API or an API Gateway URL can
 * be injected with {@link ListPublicKeysBaseUrl} Intended to be used with LocalStack.
 *
 * <p>The public key endpoint with be "publicKeysBaseUrl + /publicKeys" -- fetching the baseUrl
 * alone will result an a weird XML error.
 *
 * <p>The lambda is not deployed to LocalStack until one of those two annotations are injected.
 */
public final class LocalPublicKeyHostingServiceModule extends AbstractModule {
  private static final String PUBLIC_KEY_LAMBDA_NAME = "integration_get_public_key_lambda";

  // LINT.IfChange(public-deploy-jar)
  private static final String RELATIVE_JAR_PATH =
      "java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/PublicKeyApiGatewayHandlerLambda_deploy.jar";
  // LINT.ThenChange(BUILD:public-deploy-jar)
  private static final String HANDLER_CLASS =
      "com.google.scp.coordinator.keymanagement.keyhosting.service.aws.PublicKeyApiGatewayHandler";
  private static final String LAMBDA_JAR_KEY = "app/integration_get_public_key_lambda.jar";

  @Provides
  @Singleton
  @PublicKeyLambdaArn
  public String provideLambdaArn(
      LambdaClient lambdaClient,
      S3Client s3Client,
      @DynamoKeyDbTableName String tableName,
      LocalStackContainer localStack,
      RepoUtil repoUtil) {
    uploadLambdaCode(s3Client, LAMBDA_JAR_KEY, repoUtil.getFilePath(RELATIVE_JAR_PATH));

    return addLambdaHandler(
        lambdaClient,
        PUBLIC_KEY_LAMBDA_NAME,
        HANDLER_CLASS,
        LAMBDA_JAR_KEY,
        ImmutableMap.of(
            /* k0= */ "KEYSTORE_TABLE_NAME",
            /* v0=*/ tableName,
            /* k1=*/ "AWS_REGION",
            /* v1=*/ getRegionName(),
            /* k2=*/ "KEYSTORE_ENDPOINT_OVERRIDE",
            /* v2=*/ String.format("http://%s:4566", getLocalStackHostname(localStack))));
  }

  @Provides
  @Singleton
  @ListPublicKeysBaseUrl
  public String provideLambdaBaseUrl(
      @PublicKeyLambdaArn String lambdaArn,
      ApiGatewayClient client,
      LocalStackContainer localStack) {
    return AwsIntegrationTestUtil.createApiGatewayHandler(localStack, client, lambdaArn);
  }

  /**
   * ARN of the created lambda function. Can be used as the functionName parameter for an
   * InvokeRequest API call to Lambda.
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface PublicKeyLambdaArn {}
}
