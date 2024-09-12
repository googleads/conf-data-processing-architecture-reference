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

package com.google.scp.coordinator.keymanagement.keystorage.service.aws.testing;

import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.GetDataKeyBaseUrlOverride;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceBaseUrl;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorBKeyDbTableName;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorBWorkerKekUri;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.addLambdaHandler;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.getLocalStackHostname;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.getRegionName;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.uploadLambdaCode;

import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.BindingAnnotation;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKekUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureAlgorithm;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureKeyId;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureAlgorithm;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureKeyId;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import com.google.scp.shared.testutils.common.RepoUtil;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.nio.file.Path;
import java.util.Optional;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.services.apigateway.ApiGatewayClient;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.s3.S3Client;

/**
 * Creates a local version of KeyStorageService using LocalStack. The service is not deployed until
 * the provided {@link KeyStorageServiceBaseUrl} is injected so it should have minimal test impact
 * if unused.
 *
 * <p>Must be configured using testonly {@link CoordinatorBKeyDbTableName} and {@link
 * CoordinatorBWorkerKekUri} annotations as opposed to their corresponding coordinator-agnostic
 * annotations (DynamoKeyDbTableName, KeyEncryptionKeyUri) in order to prevent collisions in tests
 * which simulate both a Coordinator A and Coordinator B configuration.
 */
public final class LocalKeyStorageServiceModule extends AbstractModule {

  // LINT.IfChange(create-key-jar)
  private static final String RELATIVE_JAR_PATH =
      "java/com/google/scp/coordinator/keymanagement/keystorage/service/aws/KeyStorageServiceLambda_deploy.jar";
  // LINT.ThenChange(BUILD:create-key-jar)

  private static final String LAMBDA_JAR_KEY = "app/key_storage_service_lambda.jar";
  private static final String CREATE_KEY_HANDLER_CLASS =
      "com.google.scp.coordinator.keymanagement.keystorage.service.aws.CreateKeyApiGatewayHandler";
  private static final String GET_DATA_KEY_HANDLER_CLASS =
      "com.google.scp.coordinator.keymanagement.keystorage.service.aws.GetDataKeyApiGatewayHandler";

  // ensure lambda is only loaded once.
  private boolean lambdaUploaded = false;

  private void uploadLambda(S3Client s3Client, Path jarPath) {
    if (lambdaUploaded) {
      return;
    }
    lambdaUploaded = true;
    uploadLambdaCode(s3Client, LAMBDA_JAR_KEY, jarPath);
  }

  @Provides
  @Singleton
  @CreateKeyLambdaArn
  public String provideCreateKeyLambdaArn(
      LocalStackContainer localStack,
      LambdaClient lambdaClient,
      S3Client s3Client,
      @CoordinatorBKeyDbTableName String tableName,
      @CoordinatorKekUri String coordinatorKekUri,
      @CoordinatorBWorkerKekUri String workerKekUri,
      @DataKeySignatureKeyId String dataKeySignatureKeyId,
      @DataKeySignatureAlgorithm String dataKeySignatureAlgorithm,
      @EncryptionKeySignatureKeyId Optional<String> encryptionKeySignatureKeyId,
      @EncryptionKeySignatureAlgorithm String encryptionKeySignatureAlgorithm,
      RepoUtil repoUtil) {
    uploadLambda(s3Client, repoUtil.getFilePath(RELATIVE_JAR_PATH));

    var environmentVars =
        getEnvironmentVarsBuilder(
                localStack,
                coordinatorKekUri,
                dataKeySignatureKeyId,
                dataKeySignatureAlgorithm,
                encryptionKeySignatureKeyId,
                encryptionKeySignatureAlgorithm)
            .put("AWS_KMS_URI", workerKekUri)
            .put("KEYSTORE_TABLE_NAME", tableName)
            .build();

    return addLambdaHandler(
        lambdaClient,
        "create_key_lambda",
        CREATE_KEY_HANDLER_CLASS,
        LAMBDA_JAR_KEY,
        environmentVars);
  }

  @Provides
  @Singleton
  @GetDataKeyLambdaArn
  public String provideDataKeyLambdaArn(
      LocalStackContainer localStack,
      LambdaClient lambdaClient,
      S3Client s3Client,
      @CoordinatorBKeyDbTableName String tableName,
      @CoordinatorKekUri String coordinatorKekUri,
      @CoordinatorBWorkerKekUri String workerKekUri,
      @DataKeySignatureKeyId String dataKeySignatureKeyId,
      @DataKeySignatureAlgorithm String dataKeySignatureAlgorithm,
      @EncryptionKeySignatureKeyId Optional<String> encryptionKeySignatureKeyId,
      @EncryptionKeySignatureAlgorithm String encryptionKeySignatureAlgorithm,
      RepoUtil repoUtil) {
    uploadLambda(s3Client, repoUtil.getFilePath(RELATIVE_JAR_PATH));

    var environmentVars =
        getEnvironmentVarsBuilder(
                localStack,
                coordinatorKekUri,
                dataKeySignatureKeyId,
                dataKeySignatureAlgorithm,
                encryptionKeySignatureKeyId,
                encryptionKeySignatureAlgorithm)
            // Note: this is unused by the lambda but because it shares a root Guice module with
            // CreateKeyLambda, it will fail to start if this is not provided or is not in the
            // correct format.
            .put("AWS_KMS_URI", workerKekUri)
            .build();

    return addLambdaHandler(
        lambdaClient,
        "get_data_key_lambda",
        GET_DATA_KEY_HANDLER_CLASS,
        LAMBDA_JAR_KEY,
        environmentVars);
  }

  private static ImmutableMap.Builder<String, String> getEnvironmentVarsBuilder(
      LocalStackContainer localStack,
      String coordinatorKekUri,
      String dataKeySignatureKeyId,
      String dataKeySignatureAlgorithm,
      Optional<String> encryptionKeySignatureKeyId,
      String encryptionKeySignatureAlgorithm) {
    return ImmutableMap.<String, String>builder()
        .put("AWS_REGION", getRegionName())
        .put("COORDINATOR_KEK_URI", coordinatorKekUri)
        .put("DATA_KEY_SIGNATURE_KEY_ID", dataKeySignatureKeyId)
        .put("DATA_KEY_SIGNATURE_ALGORITHM", dataKeySignatureAlgorithm)
        .put("ENCRYPTION_KEY_SIGNATURE_KEY_ID", encryptionKeySignatureKeyId.orElse(""))
        .put("ENCRYPTION_KEY_SIGNATURE_ALGORITHM", encryptionKeySignatureAlgorithm)
        .put("AWS_ACCESS_KEY_ID", localStack.getAccessKey())
        .put("AWS_SECRET_ACCESS_KEY", localStack.getSecretKey())
        .put(
            "KMS_ENDPOINT_OVERRIDE",
            String.format("http://%s:4566", getLocalStackHostname(localStack)))
        .put(
            "DDB_ENDPOINT_OVERRIDE",
            String.format("http://%s:4566", getLocalStackHostname(localStack)));
  }

  @Provides
  @Singleton
  @KeyStorageServiceBaseUrl
  public String provideCreateKeyBaseUrl(
      @CreateKeyLambdaArn String lambdaArn,
      ApiGatewayClient client,
      LocalStackContainer localStack) {
    return AwsIntegrationTestUtil.createApiGatewayHandler(localStack, client, lambdaArn);
  }

  // LocalStack does not appear to support deploying routes which contain colons in it and
  // getDataKey is accessed from encryptionKeys:getDataKey. As a workaround, create 2 separate API
  // gateways that each have catch-all routes.
  @Provides
  @Singleton
  @GetDataKeyBaseUrlOverride
  public Optional<String> provideGetDataKeyBaseUrl(
      @GetDataKeyLambdaArn String lambdaArn,
      ApiGatewayClient client,
      LocalStackContainer localStack) {
    return Optional.of(
        AwsIntegrationTestUtil.createApiGatewayHandler(localStack, client, lambdaArn));
  }

  /** ARN of the CreateKey lambda, used internally to ensure the lambda is lazily loaded. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  private @interface CreateKeyLambdaArn {}

  /** ARN of the GetDataKey lambda, used internally to ensure the lambda is lazily loaded. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  private @interface GetDataKeyLambdaArn {}
}
