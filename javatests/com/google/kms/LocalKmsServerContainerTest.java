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

package com.google.kms;

import static com.google.common.truth.Truth.assertThat;
import static com.google.kms.LocalKmsConstants.DEFAULT_KEY_URI;

import com.google.crypto.tink.Aead;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

public class LocalKmsServerContainerTest {

  private LocalGcpKmsClient localGcpKmsClient;
  private String localKmsServerUrl;

  @Rule
  public LocalKmsServerContainer localKmsContainer =
      LocalKmsServerContainer.startLocalKmsContainer(
          "LocalGcpKmsServer_deploy.jar",
          "javatests/com/google/kms/",
          "com.google.kms.LocalGcpKmsServer");

  @Before
  public void setUp() {
    localKmsServerUrl = localKmsContainer.getEmulatorEndpoint();
  }

  @Test
  public void testEncryptDecryptWithLocalServer() throws Exception {
    // Test with default key uri
    localGcpKmsClient = new LocalGcpKmsClient("http://" + localKmsServerUrl);
    Aead aead = localGcpKmsClient.withDefaultCredentials().getAead(DEFAULT_KEY_URI);
    byte[] encryptBytes = "string_of_encypted_key_split".getBytes();
    byte[] response = aead.encrypt(encryptBytes, null);
    byte[] decrptBytes = aead.decrypt(response, null);
    assertThat(decrptBytes).isEqualTo(encryptBytes);
  }
}
