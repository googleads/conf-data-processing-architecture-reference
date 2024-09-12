/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.testing;

import com.beust.jcommander.JCommander;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.KeyGenerationArgs;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.Annotations.SubscriptionId;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.testing.KeyGenerationAnnotations.CloudSpannerEndpoint;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.testing.KeyGenerationAnnotations.EncodedKeysetHandle;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.testing.KeyGenerationAnnotations.KeyStorageEndpoint;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.testing.KeyGenerationAnnotations.PubSubEndpoint;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbConfig;
import javax.inject.Named;

/** Provides KeyGeneration args to start a local app for testing. */
public class KeyGenerationArgsLocalEmulatorProvider extends AbstractModule {

  @Provides
  public KeyGenerationArgs providesKeyGenerationArgs(
      @Named("CoordinatorAKeyDbConfig") SpannerKeyDbConfig spannerKeyDbConfig,
      @CloudSpannerEndpoint String spannerEndpoint,
      @PubSubEndpoint String pubSubEndpoint,
      @EncodedKeysetHandle String encodedKeysetHandle,
      @SubscriptionId String subscriptionId,
      @KeyStorageEndpoint String keyStorageEndpoint) {
    String[] args = {
      // Note: This is currently unused.
      "--kms-key-uri",
      "inline-kms://unused_so_far",
      "--spanner-instance-id",
      spannerKeyDbConfig.spannerInstanceId(),
      "--spanner-database-id",
      spannerKeyDbConfig.spannerDbName(),
      "--project-id",
      spannerKeyDbConfig.gcpProjectId(),
      "--subscription-id",
      subscriptionId,
      "--number-of-keys-to-create",
      "2",

      // Note: This is currently unused.
      "--peer-coordinator-kms-key-uri",
      "inline-kms://unused_so_far",

      // The KeyGeneration app needs to coordinate with Coordinator B to create keys.
      "--key_storage_service_base_url",
      keyStorageEndpoint,
      "--key_storage_service_cloudfunction_url",
      keyStorageEndpoint,
      "--spanner-endpoint",
      spannerEndpoint,
      "--pubsub-endpoint",
      pubSubEndpoint,
      "--test-encoded-keyset-handle",
      encodedKeysetHandle,
      "--test-peer-coordinator-encoded-keyset-handle",
      encodedKeysetHandle,
      "--multiparty",
      "--test-use-default-parameters-on-gcp"
    };

    KeyGenerationArgs keyGenerationArgs = new KeyGenerationArgs();
    JCommander.newBuilder().addObject(keyGenerationArgs).build().parse(args);
    return keyGenerationArgs;
  }
}
