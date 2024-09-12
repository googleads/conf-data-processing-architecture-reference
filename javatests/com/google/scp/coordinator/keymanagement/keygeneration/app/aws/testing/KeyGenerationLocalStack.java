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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing;

import com.google.common.collect.ImmutableMap;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.operator.cpio.cryptoclient.aws.AwsKmsHybridEncryptionKeyServiceModule.AwsKmsKeyArn;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestModule;
import java.io.IOException;
import java.util.Map;
import javax.inject.Inject;
import org.testcontainers.containers.Network;
import org.testcontainers.containers.localstack.LocalStackContainer;

/**
 * Utility class for invoking {@link KeyGenerationLambdaStarter} against LocalStack services.
 *
 * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
 */
@Deprecated
public class KeyGenerationLocalStack {

  private static final Integer VALIDITY_DAYS = 5;
  private static final Integer TTL_DAYS = 30;

  @Inject LocalStackContainer localstack;
  @Inject @DynamoKeyDbTableName String tableName;
  @Inject Network network;
  @Inject @AwsKmsKeyArn String kmsKeyArn;

  public void triggerKeyGeneration(Integer keyCount) throws IOException, InterruptedException {
    var env = getEnv(tableName, kmsKeyArn, keyCount);
    KeyGenerationLambdaStarter.runKeyRotationLambda(env, network);
  }

  /** Returns a map of environment variables necessary to start the lambda. */
  private Map<String, String> getEnv(String tableName, String keyArn, Integer keyCount) {
    return ImmutableMap.<String, String>builder()
        .put("KEYSTORE_TABLE_REGION", "us-east-1")
        .put("KEYSTORE_ENDPOINT_OVERRIDE", AwsIntegrationTestModule.getInternalEndpoint())
        .put("AWS_ACCESS_KEY_ID", localstack.getAccessKey())
        .put("AWS_SECRET_ACCESS_KEY", localstack.getSecretKey())
        .put("KMS_ENDPOINT_OVERRIDE", AwsIntegrationTestModule.getInternalEndpoint())
        .put("ENCRYPTION_KEY_ARN", keyArn)
        .put("KEYSTORE_TABLE_NAME", tableName)
        .put("KEY_GENERATION_KEY_COUNT", keyCount.toString())
        .put("KEY_GENERATION_VALIDITY_IN_DAYS", VALIDITY_DAYS.toString())
        .put("KEY_GENERATION_TTL_IN_DAYS", TTL_DAYS.toString())
        .build();
  }
}
