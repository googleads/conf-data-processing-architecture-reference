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

package com.google.scp.coordinator.keymanagement.keyhosting.service.common;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.CACHE_CONTROL_MAX;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.SECONDS_PER_DAY;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.addRandomKeysToKeyDb;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.createKey;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.createRandomKey;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static java.time.Instant.now;
import static java.time.temporal.ChronoUnit.DAYS;
import static java.util.UUID.randomUUID;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.Range;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.keymanagement.testutils.InMemoryTestEnv;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetEncryptionKeyRequestProto.GetEncryptionKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysRequestProto.ListRecentEncryptionKeysRequest;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.security.SecureRandom;
import java.util.Optional;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class KeyServiceTest {

  private static final SecureRandom RANDOM = new SecureRandom();
  @Rule public final Acai acai = new Acai(InMemoryTestEnv.class);
  @Inject private InMemoryKeyDb keyDb;
  @Inject private KeyService keyService;

  /** Adds 5 random keys to InMemoryKeyDb */
  private void setUp() {
    addRandomKeysToKeyDb(5, keyDb);
  }

  @Test
  public void getActivePublicKeys_hasAllPublicKeys() throws ServiceException {
    setUp();

    GetActivePublicKeysResponseWithHeaders response = keyService.getActivePublicKeys();

    // verify keys
    assertThat(response).isNotNull();
    ImmutableList<EncodedPublicKey> publicKeys =
        ImmutableList.copyOf(response.getActivePublicKeysResponse().getKeysList());
    assertThat(publicKeys).hasSize(5);

    // verify cache-control
    assertThat(response.cacheControlMaxAge().isPresent()).isTrue();
    String[] maxAgeParts = response.cacheControlMaxAge().get().split("=");
    assertThat(maxAgeParts).hasLength(2);
    assertThat(maxAgeParts[0]).isEqualTo("max-age");
    long sevenDaysSec = SECONDS_PER_DAY * 7;
    assertThat(Long.valueOf(maxAgeParts[1])).isIn(Range.closed(sevenDaysSec - 1000, sevenDaysSec));
  }

  @Test
  public void getActivePublicKeys_cacheControlIsExpirationRemaining() throws ServiceException {
    createKey(
        keyDb,
        /* keyId= */ randomUUID().toString(),
        /* publicKey= */ randomUUID().toString(),
        /* publicKeyMaterial= */ randomUUID().toString(),
        /* privateKey= */ randomUUID().toString(),
        /* keyEncryptionKeyUri= */ randomUUID().toString(),
        /* creationTime= */ RANDOM.nextLong(),
        /* expirationTime= */ now().plus(3, DAYS).toEpochMilli());

    GetActivePublicKeysResponseWithHeaders response = keyService.getActivePublicKeys();

    assertThat(response.getActivePublicKeysResponse().getKeysList()).hasSize(1);

    // verify cache-control similar to 3 days
    assertThat(response.cacheControlMaxAge().isPresent()).isTrue();
    String[] maxAgeParts = response.cacheControlMaxAge().get().split("=");
    long threeDaysSec = SECONDS_PER_DAY * 3;
    assertThat(Long.valueOf(maxAgeParts[1])).isIn(Range.closed(threeDaysSec - 1000, threeDaysSec));
  }

  @Test
  public void getActivePublicKeys_hasMaximumCacheControl() throws ServiceException {
    createKey(
        keyDb,
        /* keyId= */ randomUUID().toString(),
        /* publicKey= */ randomUUID().toString(),
        /* publicKeyMaterial= */ randomUUID().toString(),
        /* privateKey= */ randomUUID().toString(),
        /* keyEncryptionKeyUri= */ randomUUID().toString(),
        /* creationTime= */ RANDOM.nextLong(),
        /* expirationTime= */ now().plus(10, DAYS).toEpochMilli());

    GetActivePublicKeysResponseWithHeaders response = keyService.getActivePublicKeys();

    assertThat(response.getActivePublicKeysResponse().getKeysList()).hasSize(1);

    // verify cache-control
    assertThat(response.cacheControlMaxAge().isPresent()).isTrue();
    assertThat(response.cacheControlMaxAge().get()).isEqualTo("max-age=" + CACHE_CONTROL_MAX);
  }

  @Test
  public void getActivePublicKeys_emptyResponse() throws ServiceException {
    GetActivePublicKeysResponseWithHeaders response = keyService.getActivePublicKeys();

    assertThat(response).isNotNull();
    assertThat(response.getActivePublicKeysResponse().getKeysList()).isEmpty();
    assertThat(response.cacheControlMaxAge()).isEqualTo(Optional.empty());

    assertThat(response.cacheControlMaxAge().isPresent()).isFalse();
  }

  @Test
  public void getActivePublicKeys_errorResponse() {
    setUp();

    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));

    ServiceException exception =
        assertThrows(ServiceException.class, () -> keyService.getActivePublicKeys());
    assertThat(exception.getErrorCode()).isEqualTo(INTERNAL);
  }

  @Test
  public void getActivePublicKeys_nonServiceException() {
    // TODO: Generate a non-ServiceException to test the exception handling code.
  }

  @Test
  public void getEncryptionKey_success() throws ServiceException {
    EncryptionKey key = createRandomKey(keyDb);
    String name = "encryptionKeys/" + key.getKeyId();

    var response =
        keyService.getEncryptionKey(GetEncryptionKeyRequest.newBuilder().setName(name).build());

    assertThat(response.getName()).contains(key.getKeyId());
    assertThat(response.getKeyDataList().get(0).getKeyMaterial())
        .isEqualTo(key.getJsonEncodedKeyset());
  }

  @Test
  public void getEncryptionKey_invalidPath() {
    EncryptionKey key = createRandomKey(keyDb);

    ServiceException exception =
        assertThrows(
            ServiceException.class,
            () ->
                keyService.getEncryptionKey(
                    GetEncryptionKeyRequest.newBuilder().setName(key.getKeyId()).build()));
    assertThat(exception.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
  }

  @Test
  public void getEncryptionKey_errorResponse() {
    String name = "encryptionKeys/" + randomUUID();
    keyDb.setServiceException(new ServiceException(NOT_FOUND, SERVICE_ERROR.name(), "not found"));

    ServiceException exception =
        assertThrows(
            ServiceException.class,
            () ->
                keyService.getEncryptionKey(
                    GetEncryptionKeyRequest.newBuilder().setName(name).build()));
    assertThat(exception.getErrorCode()).isEqualTo(NOT_FOUND);
  }

  @Test
  public void listRecentKeys_happyPath_returnsExpected() throws Exception {
    // Given
    keyDb.createKey(FakeEncryptionKey.create());
    keyDb.createKey(FakeEncryptionKey.create());

    // When
    ListRecentEncryptionKeysResponse response =
        keyService.listRecentKeys(
            ListRecentEncryptionKeysRequest.newBuilder().setMaxAgeSeconds(9).build());

    // Then
    assertThat(response.getKeysList()).hasSize(2);
  }
}
