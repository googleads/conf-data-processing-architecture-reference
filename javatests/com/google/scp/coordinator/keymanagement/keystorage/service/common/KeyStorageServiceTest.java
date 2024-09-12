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

package com.google.scp.coordinator.keymanagement.keystorage.service.common;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.GetDataKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.CreateKeyRequestProto.CreateKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import com.google.scp.shared.api.exception.ServiceException;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

@RunWith(JUnit4.class)
public class KeyStorageServiceTest {

  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Inject private InMemoryKeyDb keyDb;
  @Mock CreateKeyTask mockCreateKeyTask;
  @Mock GetDataKeyTask mockGetDataKeyTask;
  private KeyStorageService keyStorageService;

  @Before
  public void getInstances() throws ServiceException {
    Answer<
            com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto
                .EncryptionKey>
        taskAnswer =
            invocation -> {
              com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto
                      .EncryptionKey
                  key =
                      (com.google.scp.coordinator.protos.keymanagement.shared.backend
                              .EncryptionKeyProto.EncryptionKey)
                          invocation.getArguments()[0];
              keyDb.createKey(key);
              return keyDb.getKey(key.getKeyId());
            };
    doAnswer(taskAnswer).when(mockCreateKeyTask).createKey(any(), any());

    keyStorageService = new KeyStorageService(mockCreateKeyTask, mockGetDataKeyTask);
  }

  @Test
  public void handleRequest_savePublicKey() throws ServiceException {
    String keyId = "12345";
    String name = "keys/" + keyId;
    EncryptionKey key =
        EncryptionKey.newBuilder()
            .setName(name)
            .setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY)
            .setPublicKeysetHandle("")
            .setPublicKeyMaterial("qwert")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .addAllKeyData(ImmutableList.of())
            .build();

    keyStorageService.createKey(CreateKeyRequest.newBuilder().setKeyId(keyId).setKey(key).build());

    assertThat(keyDb.getKey(keyId).getPublicKeyMaterial()).isEqualTo("qwert");
  }

  @Test
  public void handleRequest_savePublicKeyWithOverwrite() throws ServiceException {
    String keyId = "12345";
    String name = "keys/" + keyId;
    String publicKey1 = "asdf";
    String publicKey2 = "ghjk";
    EncryptionKey key1 =
        EncryptionKey.newBuilder()
            .setName(name)
            .setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY)
            .setPublicKeysetHandle("")
            .setPublicKeyMaterial(publicKey1)
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .addAllKeyData(ImmutableList.of())
            .build();
    EncryptionKey key2 =
        EncryptionKey.newBuilder()
            .setName(name)
            .setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY)
            .setPublicKeysetHandle("")
            .setPublicKeyMaterial(publicKey2)
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .addAllKeyData(ImmutableList.of())
            .build();

    keyStorageService.createKey(CreateKeyRequest.newBuilder().setKeyId(keyId).setKey(key1).build());
    keyStorageService.createKey(CreateKeyRequest.newBuilder().setKeyId(keyId).setKey(key2).build());

    assertThat(keyDb.getKey(keyId).getPublicKeyMaterial()).isEqualTo(publicKey2);
  }

  @Test
  public void handleRequest_emptyKeyId() {
    String keyId = "";
    String name = "keys/" + keyId;
    EncryptionKey key =
        EncryptionKey.newBuilder()
            .setName(name)
            .setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY)
            .setPublicKeysetHandle("")
            .setPublicKeyMaterial("qwert")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .addAllKeyData(ImmutableList.of())
            .build();

    var createKeyRequest = CreateKeyRequest.newBuilder().setKeyId(keyId).setKey(key).build();
    ServiceException ex =
        assertThrows(ServiceException.class, () -> keyStorageService.createKey(createKeyRequest));

    assertThat(ex.getErrorCode()).isEqualTo(INVALID_ARGUMENT);
    assertThat(ex.getErrorReason()).isEqualTo(INVALID_ARGUMENT.name());
    assertThat(ex.getMessage()).isEqualTo("KeyId cannot be null or empty.");
  }

  @Test
  public void handleRequest_errorResponse() {
    EncryptionKey key =
        EncryptionKey.newBuilder()
            .setName("keys/asdf")
            .setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY)
            .setPublicKeysetHandle("12345")
            .setPublicKeyMaterial("qwert")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .addAllKeyData(ImmutableList.of())
            .build();
    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));

    var createKeyRequest = CreateKeyRequest.newBuilder().setKeyId("id").setKey(key).build();
    assertThrows(ServiceException.class, () -> keyStorageService.createKey(createKeyRequest));
  }
}
