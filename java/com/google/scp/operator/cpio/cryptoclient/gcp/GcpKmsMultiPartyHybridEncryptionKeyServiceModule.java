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

package com.google.scp.operator.cpio.cryptoclient.gcp;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.crypto.tink.integration.gcpkms.GcpKmsClient;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorAHttpClient;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBHttpClient;
import com.google.scp.operator.cpio.configclient.gcp.Annotations.CoordinatorACredentials;
import com.google.scp.operator.cpio.configclient.gcp.Annotations.CoordinatorBCredentials;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAAead;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAEncryptionKeyServiceBaseUrl;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorBAead;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorBEncryptionKeyServiceBaseUrl;
import com.google.scp.operator.cpio.cryptoclient.EncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.HttpEncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.HybridEncryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.HybridEncryptionKeyServiceModule;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyHybridEncryptionKeyServiceImpl;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyHybridEncryptionKeyServiceImpl.CoordinatorAEncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyHybridEncryptionKeyServiceImpl.CoordinatorBEncryptionKeyFetchingService;
import com.google.scp.shared.api.util.HttpClientWrapper;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import java.security.GeneralSecurityException;

/**
 * Guice module for implementation of {@link HybridEncryptionKeyService} which fetches private key
 * splits from multiple parties and decrypts the private key splits using gcloud KMS.
 */
public final class GcpKmsMultiPartyHybridEncryptionKeyServiceModule
    extends HybridEncryptionKeyServiceModule {

  /** Caller is expected to bind {@link GcpKmsHybridEncryptionKeyServiceConfig}. */
  public GcpKmsMultiPartyHybridEncryptionKeyServiceModule() {}

  @Override
  public Class<? extends HybridEncryptionKeyService> getHybridEncryptionKeyServiceImplementation() {
    return MultiPartyHybridEncryptionKeyServiceImpl.class;
  }

  @Provides
  @CoordinatorAEncryptionKeyFetchingService
  @Singleton
  public EncryptionKeyFetchingService provideCoordinatorAEncryptionKeyFetchingService(
      @CoordinatorAHttpClient HttpClientWrapper httpClient,
      @CoordinatorAEncryptionKeyServiceBaseUrl String coordinatorAEncryptionKeyServiceBaseUrl) {
    return new HttpEncryptionKeyFetchingService(
        httpClient, coordinatorAEncryptionKeyServiceBaseUrl);
  }

  @Provides
  @CoordinatorBEncryptionKeyFetchingService
  @Singleton
  public EncryptionKeyFetchingService provideCoordinatorBEncryptionKeyFetchingService(
      @CoordinatorBHttpClient HttpClientWrapper httpClient,
      @CoordinatorBEncryptionKeyServiceBaseUrl String coordinatorBEncryptionKeyServiceBaseUrl) {
    return new HttpEncryptionKeyFetchingService(
        httpClient, coordinatorBEncryptionKeyServiceBaseUrl);
  }

  /** Provides a {@code KmsClient} for coordinator A. */
  @Provides
  @Singleton
  @CoordinatorAAead
  CloudAeadSelector provideCoordinatorAKmsClient(
      @CoordinatorACredentials GoogleCredentials credentials,
      GcpKmsHybridEncryptionKeyServiceConfig gcpKmsHybridEncryptionKeyServiceConfig) {
    if (gcpKmsHybridEncryptionKeyServiceConfig.coordinatorAAead().isPresent()) {
      return (unused) -> gcpKmsHybridEncryptionKeyServiceConfig.coordinatorAAead().get();
    }
    return (kmsKeyArn) -> {
      GcpKmsClient client = new GcpKmsClient();
      try {
        client.withCredentials(credentials);
        return client.getAead(kmsKeyArn);
      } catch (GeneralSecurityException e) {
        throw new RuntimeException(
            String.format("Error getting gcloud Aead with uri %s.", kmsKeyArn), e);
      }
    };
  }

  /** Provides a {@code KmsClient} for coordinator B. */
  @Provides
  @Singleton
  @CoordinatorBAead
  CloudAeadSelector provideCoordinatorBKmsClient(
      @CoordinatorBCredentials GoogleCredentials credentials,
      GcpKmsHybridEncryptionKeyServiceConfig gcpKmsHybridEncryptionKeyServiceConfig) {
    if (gcpKmsHybridEncryptionKeyServiceConfig.coordinatorBAead().isPresent()) {
      return (unused) -> gcpKmsHybridEncryptionKeyServiceConfig.coordinatorBAead().get();
    }
    return (kmsKeyArn) -> {
      GcpKmsClient client = new GcpKmsClient();
      try {
        client.withCredentials(credentials);
        return client.getAead(kmsKeyArn);
      } catch (GeneralSecurityException e) {
        throw new RuntimeException(
            String.format("Error getting gcloud Aead with uri %s.", kmsKeyArn), e);
      }
    };
  }
}
