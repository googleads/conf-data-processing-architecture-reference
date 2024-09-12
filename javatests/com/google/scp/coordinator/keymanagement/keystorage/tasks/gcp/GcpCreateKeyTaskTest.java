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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.KEY_LIMIT;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.acai.Acai;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AeadConfig;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.security.GeneralSecurityException;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Base64;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.function.ThrowingRunnable;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class GcpCreateKeyTaskTest {
  private static final String KEK_URI = "gcp-kms://kek-uri";
  private static final byte[] TEST_PUBLIC_KEY = "test private key split".getBytes();
  private static final byte[] TEST_PRIVATE_KEY = "test public key".getBytes();
  private static Aead TEST_AEAD;

  @Rule public final Acai acai = new Acai(GcpKeyStorageTestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Inject private InMemoryKeyDb keyDb;
  @Mock Aead mockAead;

  @BeforeClass
  public static void setUp() throws Exception {
    AeadConfig.register();
    TEST_AEAD = KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM")).getPrimitive(Aead.class);
  }

  @Test
  public void createKey_success() throws ServiceException, GeneralSecurityException {
    when(mockAead.decrypt(any(), any())).thenReturn("decryptSuccessful".getBytes());
    when(mockAead.encrypt(any(), any())).thenReturn("encryptSuccessful".getBytes());
    var encryptedPrivateKeySplit = toBase64("12345");
    CreateKeyTask taskWithMock = new GcpCreateKeyTask(keyDb, mockAead, KEK_URI);
    String keyId = "asdf";
    var creationTime = Instant.now().toEpochMilli();
    var expirationTime = Instant.now().plus(1, ChronoUnit.DAYS).toEpochMilli();
    EncryptionKey key =
        FakeEncryptionKey.create().toBuilder()
            .setKeyId(keyId)
            .setPublicKey("67890")
            .setExpirationTime(expirationTime)
            .setCreationTime(creationTime)
            .build();

    taskWithMock.createKey(key, encryptedPrivateKeySplit);

    com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey
        resultPublic = keyDb.getActiveKeys(KEY_LIMIT).get(0);
    com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey
        resultPrivate = keyDb.getKey(keyId);
    assertThat(resultPublic.getKeyId()).isEqualTo(keyId);
    assertThat(resultPublic.getPublicKey()).isEqualTo(key.getPublicKey());
    assertThat(resultPublic.getCreationTime()).isEqualTo(creationTime);
    assertThat(resultPublic.getExpirationTime()).isEqualTo(expirationTime);
    assertThat(resultPrivate.getKeyId()).isEqualTo(keyId);
    assertThat(resultPrivate.getJsonEncodedKeyset()).isEqualTo(toBase64("encryptSuccessful"));
    assertThat(resultPrivate.getExpirationTime()).isEqualTo(expirationTime);
    assertThat(resultPrivate.getCreationTime()).isEqualTo(creationTime);
    assertThat(resultPrivate.getKeyEncryptionKeyUri()).isEqualTo(KEK_URI);
    assertThat(resultPrivate.getKeySplitDataList().get(0).getKeySplitKeyEncryptionKeyUri())
        .isEqualTo(key.getKeySplitDataList().get(0).getKeySplitKeyEncryptionKeyUri());
    // Test data has 2 keysplitdata items, so the new one would be after
    assertThat(resultPrivate.getKeySplitDataList().get(2).getKeySplitKeyEncryptionKeyUri())
        .isEqualTo(KEK_URI);
  }

  @Test
  public void createKey_successOverwrite() throws ServiceException, GeneralSecurityException {
    when(mockAead.decrypt(any(), any())).thenReturn("decryptSuccessful".getBytes());
    when(mockAead.encrypt(any(), any())).thenReturn("encryptSuccessful".getBytes());
    CreateKeyTask taskWithMock = new GcpCreateKeyTask(keyDb, mockAead, KEK_URI);
    String keyId = "asdf";
    var creationTime = 500L;
    var split1 = toBase64("12345");
    var split2 = toBase64("67890");
    EncryptionKey key1 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("")
            .setExpirationTime(0L)
            .setCreationTime(creationTime)
            .build();
    EncryptionKey key2 =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey("")
            .setExpirationTime(0L)
            .setCreationTime(creationTime)
            .build();

    taskWithMock.createKey(key1, split1);
    taskWithMock.createKey(key2, split2);

    com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey
        result = keyDb.getKey(keyId);
    assertThat(result.getKeyId()).isEqualTo(keyId);
    assertThat(result.getPublicKey()).isEqualTo(key2.getPublicKey());
    assertThat(result.getJsonEncodedKeyset()).isEqualTo(toBase64("encryptSuccessful"));
    assertThat(result.getCreationTime()).isEqualTo(creationTime);
    assertThat(result.getExpirationTime()).isEqualTo(key2.getExpirationTime());
  }

  @Test
  public void createKey_databaseException() throws GeneralSecurityException {
    when(mockAead.decrypt(any(), any())).thenReturn("decryptSuccessful".getBytes());
    when(mockAead.encrypt(any(), any())).thenReturn("encryptSuccessful".getBytes());
    CreateKeyTask taskWithMock = new GcpCreateKeyTask(keyDb, mockAead, KEK_URI);
    EncryptionKey key =
        EncryptionKey.newBuilder().setKeyId("asdf").setPublicKey("").setExpirationTime(0L).build();
    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));

    ServiceException ex =
        assertThrows(ServiceException.class, () -> taskWithMock.createKey(key, toBase64("123")));

    assertThat(ex.getErrorCode()).isEqualTo(INTERNAL);
    verify(mockAead).decrypt(any(), any());
  }

  @Test
  public void createKey_validationException() throws ServiceException, GeneralSecurityException {
    when(mockAead.decrypt(any(), any())).thenThrow(new GeneralSecurityException());
    com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask taskWithMock =
        new GcpCreateKeyTask(keyDb, mockAead, KEK_URI);
    EncryptionKey key =
        EncryptionKey.newBuilder()
            .setKeyId("myName")
            .setPublicKey("123")
            .setExpirationTime(0L)
            .build();

    ServiceException ex =
        assertThrows(ServiceException.class, () -> taskWithMock.createKey(key, toBase64("456")));

    assertThat(ex).hasCauseThat().isInstanceOf(GeneralSecurityException.class);
    assertThat(ex.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    assertThat(keyDb.getActiveKeys(5).size()).isEqualTo(0);
    verify(mockAead).decrypt(any(), any());
  }

  @Test
  public void createKey_missingPrivateKeyException()
      throws ServiceException, GeneralSecurityException {
    when(mockAead.decrypt(any(), any())).thenReturn("decryptSuccessful".getBytes());
    CreateKeyTask taskWithMock = new GcpCreateKeyTask(keyDb, mockAead, KEK_URI);
    EncryptionKey key =
        EncryptionKey.newBuilder()
            .setKeyId("myName")
            .setPublicKey("123")
            .setExpirationTime(0L)
            .build();

    ServiceException ex =
        assertThrows(ServiceException.class, () -> taskWithMock.createKey(key, ""));

    assertThat(ex.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    assertThat(keyDb.getActiveKeys(5).size()).isEqualTo(0);
    verify(mockAead, never()).decrypt(any(), any());
  }

  @Test
  public void createKey_happyInput_createsExpected() throws Exception {
    // Given
    var encryptedPrivateKeyWithAssociatedPublicKey =
        Base64.getEncoder().encodeToString(TEST_AEAD.encrypt(TEST_PUBLIC_KEY, TEST_PRIVATE_KEY));
    var key =
        FakeEncryptionKey.create().toBuilder()
            .setPublicKeyMaterial(Base64.getEncoder().encodeToString(TEST_PRIVATE_KEY))
            .build();

    // When
    var task = new GcpCreateKeyTask(keyDb, TEST_AEAD, KEK_URI);
    task.createKey(key, encryptedPrivateKeyWithAssociatedPublicKey);

    // Then
    var storedKey = keyDb.getKey(key.getKeyId());
    var storedPrivateKey =
        TEST_AEAD.decrypt(Base64.getDecoder().decode(storedKey.getJsonEncodedKeyset()), null);
    assertThat(storedPrivateKey).isEqualTo(TEST_PUBLIC_KEY);
  }

  @Test
  public void createKey_mismatchPublicKey_throwsExpectedError() throws Exception {
    // Given
    var encryptedPrivateKeyWithAssociatedPublicKey =
        Base64.getEncoder()
            .encodeToString(TEST_AEAD.encrypt(TEST_PUBLIC_KEY, "mismatch".getBytes()));
    var key =
        FakeEncryptionKey.create().toBuilder()
            .setPublicKeyMaterial(Base64.getEncoder().encodeToString(TEST_PRIVATE_KEY))
            .build();

    // When
    var task = new GcpCreateKeyTask(keyDb, TEST_AEAD, KEK_URI);
    ThrowingRunnable when = () -> task.createKey(key, encryptedPrivateKeyWithAssociatedPublicKey);

    // Then
    var exception = assertThrows(ServiceException.class, when);
    assertThat(exception).hasMessageThat().contains("Key-split validation failed");
  }

  /** Small helper function for generating valid base64 from a string literal. */
  private static String toBase64(String input) {
    return Base64.getEncoder().encodeToString(input.getBytes());
  }
}
