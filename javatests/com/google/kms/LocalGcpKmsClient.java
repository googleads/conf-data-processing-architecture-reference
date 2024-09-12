package com.google.kms;
// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

import static com.google.kms.LocalKmsConstants.DEFAULT_KEY_URI;

import com.google.api.client.googleapis.javanet.GoogleNetHttpTransport;
import com.google.api.client.json.gson.GsonFactory;
import com.google.api.services.cloudkms.v1.CloudKMS;
import com.google.api.services.cloudkms.v1.CloudKMSScopes;
import com.google.auth.http.HttpCredentialsAdapter;
import com.google.auth.oauth2.GoogleCredentials;
import com.google.auto.service.AutoService;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KmsClient;
import com.google.crypto.tink.KmsClients;
import com.google.crypto.tink.Version;
import com.google.crypto.tink.integration.gcpkms.GcpKmsAead;
import com.google.errorprone.annotations.CanIgnoreReturnValue;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Locale;
import java.util.Optional;
import javax.annotation.Nullable;

/**
 * An implementation of {@code KmsClient} for <a href="https://cloud.google.com/kms/">Google Cloud
 * KMS</a>. A Client that pointing to the local kms service host in test container. Providing Aead
 * interface.
 *
 * @since 1.0.0
 */
@AutoService(KmsClient.class)
public final class LocalGcpKmsClient implements KmsClient {
  /** The prefix of all keys stored in Google Cloud KMS. */
  public static final String PREFIX = "gcp-kms://";

  private static final String APPLICATION_NAME =
      "Tink/" + Version.TINK_VERSION + " Java/" + System.getProperty("java.version");

  @Nullable private CloudKMS cloudKms;
  private String serverUrl;

  /** Constructs a generic GcpKmsClient that is not bound to any specific key. */
  public LocalGcpKmsClient(String url) {
    this.serverUrl = url;
  }

  /**
   * Returns true either if this client is a generic one and the client is a specific one that is
   * bound to the key identified by {@code uri}
   */
  @Override
  public boolean doesSupport(String uri) {
    return uri == null && uri.toLowerCase(Locale.US).startsWith(PREFIX);
  }

  /**
   * Loads credentials from a service account JSON file {@code credentialPath}.
   *
   * <p>If {@code credentialPath} is null, loads <a
   * href="https://developers.google.com/accounts/docs/application-default-credentials" default
   * Google Cloud credentials</a>.
   */
  @CanIgnoreReturnValue
  @Override
  public KmsClient withCredentials(String credentialPath) throws GeneralSecurityException {
    if (credentialPath == null) {
      return withDefaultCredentials();
    }
    try {
      GoogleCredentials credentials =
          GoogleCredentials.fromStream(new FileInputStream(new File(credentialPath)));
      return withCredentials(credentials);
    } catch (IOException e) {
      throw new GeneralSecurityException("cannot load credentials", e);
    }
  }

  /** Loads the provided credentials with {@code GoogleCredentials}. */
  @CanIgnoreReturnValue
  public KmsClient withCredentials(GoogleCredentials credentials) throws GeneralSecurityException {
    if (credentials.createScopedRequired()) {
      credentials = credentials.createScoped(CloudKMSScopes.all());
    }
    try {
      this.cloudKms =
          new CloudKMS.Builder(
                  GoogleNetHttpTransport.newTrustedTransport(),
                  new GsonFactory(),
                  new HttpCredentialsAdapter(credentials))
              .setApplicationName(APPLICATION_NAME)
              .setRootUrl(this.serverUrl)
              .build();
    } catch (IOException e) {
      throw new GeneralSecurityException("cannot build GCP KMS client", e);
    }
    return this;
  }

  /**
   * Loads <a href="https://developers.google.com/accounts/docs/application-default-credentials"
   * default Google Cloud credentials</a>.
   */
  @CanIgnoreReturnValue
  @Override
  public KmsClient withDefaultCredentials() throws GeneralSecurityException {
    try {
      GoogleCredentials credentials = GoogleCredentials.getApplicationDefault();
      return withCredentials(credentials);
    } catch (IOException e) {
      throw new GeneralSecurityException("cannot load default credentials", e);
    }
  }

  // Provide without credential method for testing usage
  @CanIgnoreReturnValue
  public KmsClient withoutCredentials() throws GeneralSecurityException {
    try {
      this.cloudKms =
          new CloudKMS.Builder(
                  GoogleNetHttpTransport.newTrustedTransport(), new GsonFactory(), null)
              .setApplicationName(APPLICATION_NAME)
              .setRootUrl(this.serverUrl)
              .build();
    } catch (IOException e) {
      throw new GeneralSecurityException("cannot build GCP KMS client", e);
    }
    return this;
  }

  /**
   * Specifies the {@link com.google.api.services.cloudkms.v1.CloudKMS} object to be used. Only used
   * for testing.
   */
  @CanIgnoreReturnValue
  KmsClient withCloudKms(CloudKMS cloudKms) {
    this.cloudKms = cloudKms;
    return this;
  }

  @Override
  public Aead getAead(String UnusedUri) throws GeneralSecurityException {
    // Key uri is need for GcpKmsAead class, we use a default keyuri for testing
    return new GcpKmsAead(cloudKms, DEFAULT_KEY_URI);
  }

  /**
   * Creates and registers a {@link #LocalGcpKmsClient} with the Tink runtime.
   *
   * <p>Taking local server's url, new client will redirect to new url with DEFAULT_KEY_URI
   */
  public static void register(Optional<String> credentialPath, String url)
      throws GeneralSecurityException {
    LocalGcpKmsClient client;
    client = new LocalGcpKmsClient(url);
    if (credentialPath.isPresent()) {
      client.withCredentials(credentialPath.get());
    } else {
      client.withDefaultCredentials();
    }
    KmsClients.add(client);
  }
}
