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

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.kms.LocalKmsConstants.DEFAULT_KEY_URI;
import static com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil.SPANNER_KEY_TABLE_NAME;
import static com.google.scp.shared.testutils.common.HttpRequestUtil.executeRequestWithRetry;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.KeySet;
import com.google.cloud.spanner.Mutation;
import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeysetHandle;
import com.google.gson.Gson;
import com.google.inject.Inject;
import com.google.kms.LocalGcpKmsClient;
import com.google.kms.LocalKmsServerContainer;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.KeyStorageCloudFunctionContainerWithKms;
import com.google.scp.coordinator.keymanagement.testutils.gcp.Annotations.TestLocalKmsServerContainer;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import com.google.scp.shared.util.Base64Util;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class KeyStorageServiceIntegrationTest {

  @Rule public final Acai acai = new Acai(GcpKeyStorageServiceTestEnvModule.class);

  private static final HttpClient CLIENT = HttpClient.newHttpClient();

  private static final String KEY_STORAGE_PATH = "/v1alpha/encryptionKeys";

  @Inject @KeyStorageCloudFunctionContainerWithKms
  private CloudFunctionEmulatorContainer functionContainer;

  @Inject private SpannerKeyDb keyDb;

  @Inject private KeysetHandle keysetHandle;

  // DatabaseClient is lazy loaded, so it is needed to inject this here so that it triggers the
  // creation of the spanner db; otherwise, it will not exist when the cloud function tries to
  // access it.
  @Inject @KeyDbClient private DatabaseClient dbClient;

  @Inject @TestLocalKmsServerContainer private LocalKmsServerContainer localKmsServerContainer;

  public void cleanUp() {
    dbClient.write(ImmutableList.of(Mutation.delete(SPANNER_KEY_TABLE_NAME, KeySet.all())));
  }

  @Test
  public void createKeys_success() throws Exception {
    String keyId = "12345";
    String localKmsServerUrl = "http://" + localKmsServerContainer.getEmulatorEndpoint();
    LocalGcpKmsClient localGcpKmsClient = new LocalGcpKmsClient(localKmsServerUrl);
    Aead aead = localGcpKmsClient.withoutCredentials().getAead(DEFAULT_KEY_URI);
    ByteString encryptedKeySplit =
        ByteString.copyFrom(
            aead.encrypt(
                "string_of_encypted_key_split".getBytes(), "a_public_key_material".getBytes()));
    HttpRequest request =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(KEY_STORAGE_PATH))
            .POST(
                HttpRequest.BodyPublishers.ofString(
                    getRequestBodyInJson(Base64Util.toBase64String(encryptedKeySplit), keyId)))
            .setHeader("Content-Type", "application/json")
            .build();

    HttpResponse<String> response = executeRequestWithRetry(CLIENT, request);

    assertThat(response.statusCode()).isEqualTo(200);
    assertThat(keyDb.getKey(keyId).getJsonEncodedKeyset()).isNotNull();
  }

  @Test
  public void createKeys_incorrectEnecryptedKeyFailure() throws Exception {
    String keyId = "6789";
    ByteString nonEncryptedKeySplit =
        ByteString.copyFrom("string_of_encypted_key_split".getBytes());

    HttpRequest request =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(KEY_STORAGE_PATH))
            .POST(
                HttpRequest.BodyPublishers.ofString(
                    getRequestBodyInJson(Base64Util.toBase64String(nonEncryptedKeySplit), keyId)))
            .setHeader("Content-Type", "application/json")
            .build();

    HttpResponse<String> response = executeRequestWithRetry(CLIENT, request);

    assertThat(response.statusCode()).isEqualTo(400);
    assertThrows(ServiceException.class, () -> keyDb.getKey(keyId));
  }

  private String getRequestBodyInJson(String encryptedKeysplit, String keyId) {
    Gson gson = new Gson();

    Map<String, String> keyData = new HashMap<>();
    keyData.put("publicKeySignature", "a");
    keyData.put("keyEncryptionKeyUri", "b");
    keyData.put("keyMaterial", "c");

    Map<String, Object> key = new HashMap<>();
    key.put("name", "keys/" + keyId);
    key.put("encryptionKeyType", "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT");
    key.put("publicKeysetHandle", "a_public_keyset_handle");
    key.put(
        "publicKeyMaterial",
        Base64.getEncoder().encodeToString("a_public_key_material".getBytes()));
    key.put("creationTime", "0");
    key.put("expirationTime", "0");
    key.put("keyData", new Object[] {keyData});

    Map<String, Object> createKeyRequest = new HashMap<>();
    createKeyRequest.put("keyId", keyId);
    createKeyRequest.put("encryptedKeySplit", encryptedKeysplit);
    createKeyRequest.put("keySplitEncryptionType", "DIRECT");
    createKeyRequest.put("key", key);

    return gson.toJson(createKeyRequest);
  }

  private URI getFunctionUri(String path) {
    return URI.create("http://" + functionContainer.getEmulatorEndpoint() + path);
  }
}
