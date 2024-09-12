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

import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsUrl;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.GetDataKeyBaseUrlOverride;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceBaseUrl;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAEncryptionKeySignatureAlgorithm;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAEncryptionKeySignatureKeyId;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAKeyDbTableName;
import static com.google.scp.coordinator.keymanagement.testutils.aws.Annotations.CoordinatorAWorkerKekUri;

import com.beust.jcommander.JCommander;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.AwsSplitKeyGenerationArgs;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyIdTypeName;
import java.util.Optional;
import org.testcontainers.containers.localstack.LocalStackContainer;

/**
 * Helper module which can be installed to provide the necessary {@link AwsSplitKeyGenerationArgs}
 * to run a {@link SplitKeyGenerationStarter} against LocalStack services.
 */
public final class SplitKeyGenerationArgsLocalStackProvider extends AbstractModule {

  /** Overide the default 5 with 2 to speed up integration tests. */
  public static final int KEY_COUNT = 2;

  @Provides
  public AwsSplitKeyGenerationArgs provideSplitKeyGenerationArgs(
      @KeyGenerationSqsUrl String queueUrl,
      @CoordinatorAWorkerKekUri String coordAKek,
      @CoordinatorAKeyDbTableName String coordATable,
      @KeyStorageServiceBaseUrl String keyStorageServiceUrl,
      @GetDataKeyBaseUrlOverride Optional<String> getDataKeyBaseUrl,
      @CoordinatorAEncryptionKeySignatureKeyId Optional<String> signatureKeyId,
      @CoordinatorAEncryptionKeySignatureAlgorithm String signatureAlgorithm,
      @KeyIdTypeName Optional<String> keyIdTypeName,
      LocalStackContainer localStack) {
    String[] args = {
      // Needed when Ec2MetadataUtils cannot fetch region.
      "--application_region_override",
      "us-east-1",
      "--key_id_type",
      keyIdTypeName.orElse(""),
      "--key_db_name",
      coordATable,
      "--key_db_region",
      "us-east-1",
      "--key_generation_queue_url",
      queueUrl,
      "--key_storage_service_base_url",
      keyStorageServiceUrl,
      "--get_data_key_override_base_url",
      getDataKeyBaseUrl.get(),
      "--kms_key_uri",
      coordAKek,
      "--signature_key_id",
      signatureKeyId.orElse(""),
      "--signature_algorithm",
      signatureAlgorithm,
      "--key_generation_queue_max_wait_time_seconds",
      "20",
      "--key_count",
      String.valueOf(KEY_COUNT),
      "--sqs_endpoint_override",
      localStack.getEndpointOverride(LocalStackContainer.Service.SQS).toString(),
      "--kms_endpoint_override",
      localStack.getEndpointOverride(LocalStackContainer.Service.KMS).toString(),
      "--key_db_endpoint_override",
      localStack.getEndpointOverride(LocalStackContainer.Service.DYNAMODB).toString(),
      "--sts_endpoint_override",
      localStack.getEndpointOverride(LocalStackContainer.Service.STS).toString(),
    };

    AwsSplitKeyGenerationArgs params = new AwsSplitKeyGenerationArgs();
    JCommander.newBuilder().addObject(params).build().parse(args);
    return params;
  }
}
