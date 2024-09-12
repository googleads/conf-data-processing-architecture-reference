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

import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAKeyDbTableName;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorBKeyDbTableName;
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
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import com.google.scp.shared.testutils.common.RepoUtil;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.services.apigateway.ApiGatewayClient;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.s3.S3Client;

/**
 * Creates 2 ListRecentEncryptionKeyHosting services using LocalStack to simulate a Split
 * Coordinator environments for integration tests with multiple KeyDb tables.
 *
 * <p>Localstack API gateway doesn't support one endpoint for two lambdas, so we create another
 * endpoint for ListRecentEncryptionKeys other than reusing the one for GetEncryptionKey.
 */
public final class MultiLocalListRecentEncryptionKeysHostingServiceModule extends AbstractModule {
  // LINT.IfChange(encryption-deploy-jar)
  private static final String RELATIVE_JAR_PATH =
      "java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/EncryptionKeyServiceLambda_deploy.jar";
  // LINT.ThenChange(BUILD:encryption-deploy-jar)
  private static final String LAMBDA_JAR_KEY = "app/list_recent_encryption_key_service_lambda.jar";
  private static final String HANDLER_CLASS =
      "com.google.scp.coordinator.keymanagement.keyhosting.service.aws.ListRecentEncryptionKeysApiGatewayHandler";
  // ensure lambda is only loaded once.
  private boolean lambdaLoaded = false;

  private void uploadLambda(S3Client s3Client, RepoUtil repoUtil) {
    if (lambdaLoaded) {
      return;
    }
    lambdaLoaded = true;
    uploadLambdaCode(s3Client, LAMBDA_JAR_KEY, repoUtil.getFilePath(RELATIVE_JAR_PATH));
  }

  @Provides
  @Singleton
  @PrimaryListRecentEncryptionKeysLambdaArn
  public String provideLambdaArn1(
      LocalStackContainer localStack,
      LambdaClient lambdaClient,
      S3Client s3Client,
      @CoordinatorAKeyDbTableName String tableName,
      RepoUtil repoUtil) {
    uploadLambda(s3Client, repoUtil);
    return createLambdaForTable(localStack, lambdaClient, s3Client, tableName);
  }

  @Provides
  @Singleton
  @SecondaryListRecentEncryptionKeysLambdaArn
  public String provideLambdaArn2(
      LocalStackContainer localStack,
      LambdaClient lambdaClient,
      S3Client s3Client,
      @CoordinatorBKeyDbTableName String tableName,
      RepoUtil repoUtil) {
    uploadLambda(s3Client, repoUtil);
    return createLambdaForTable(localStack, lambdaClient, s3Client, tableName);
  }

  /**
   * @return The ARN of the created lambda.
   */
  private static String createLambdaForTable(
      LocalStackContainer localStack,
      LambdaClient lambdaClient,
      S3Client s3Client,
      String tableName) {
    return addLambdaHandler(
        lambdaClient,
        "integration_list_recent_encryption_keys_lambda_" + tableName,
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
  @CoordinatorAListRecentEncryptionKeysServiceBaseUrl
  public String providePrimaryEncryptionKeyServiceBaseUrl(
      @PrimaryListRecentEncryptionKeysLambdaArn String lambdaArn,
      ApiGatewayClient client,
      LocalStackContainer localStack) {
    return AwsIntegrationTestUtil.createApiGatewayHandler(localStack, client, lambdaArn);
  }

  @Provides
  @Singleton
  @CoordinatorBListRecentEncryptionKeysServiceBaseUrl
  public String provideSecondaryEncryptionKeyServiceBaseUrl(
      @SecondaryListRecentEncryptionKeysLambdaArn String lambdaArn,
      ApiGatewayClient client,
      LocalStackContainer localStack) {
    return AwsIntegrationTestUtil.createApiGatewayHandler(localStack, client, lambdaArn);
  }

  /** Base URL where coordinator A's list recent encryption key vending service is located. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorAListRecentEncryptionKeysServiceBaseUrl {}

  /** Base URL where coordinator B's list recent encryption key vending service is located. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorBListRecentEncryptionKeysServiceBaseUrl {}

  /** ARN of lambda 1, used internally to ensure the lambda is lazily loaded. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PrimaryListRecentEncryptionKeysLambdaArn {}

  /** ARN of lambda 2, used internally to ensure the lambda is lazily loaded. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface SecondaryListRecentEncryptionKeysLambdaArn {}
}
