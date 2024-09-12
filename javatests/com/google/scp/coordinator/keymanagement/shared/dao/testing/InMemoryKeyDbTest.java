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

package com.google.scp.coordinator.keymanagement.shared.dao.testing;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.coordinator.keymanagement.testutils.DynamoKeyDbTestUtil.KEY_LIMIT;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.fail;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDbBaseTest;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.util.stream.IntStream;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class InMemoryKeyDbTest extends KeyDbBaseTest {

  @Rule public final Acai acai = new Acai(TestEnv.class);

  private static final class TestEnv extends AbstractModule {
    @Override
    protected void configure() {
      bind(KeyDb.class).to(InMemoryKeyDb.class);
    }
  }

  @Test
  public void getActiveKeys_getAllInDb() throws ServiceException {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();
    IntStream.range(0, 5).forEach(unused -> createRandomKey(keyDb));

    ImmutableList<EncryptionKey> keys = keyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).hasSize(5);
  }

  @Test
  public void getActiveKeys_exception() {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();
    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));

    assertThrows(ServiceException.class, () -> keyDb.getActiveKeys(KEY_LIMIT));
  }

  @Test
  public void reset() throws ServiceException {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();
    IntStream.range(0, 5).forEach(unused -> createRandomKey(keyDb));
    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));
    keyDb.reset();

    ImmutableList<EncryptionKey> keys = keyDb.getActiveKeys(KEY_LIMIT);

    assertThat(keys).isEmpty();
  }

  @Test
  public void getKey_success() throws ServiceException {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();
    EncryptionKey key =
        EncryptionKey.newBuilder()
            .setKeyId("abcd")
            .setJsonEncodedKeyset("{key}")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();
    keyDb.createKey(key);

    EncryptionKey receivedKey = keyDb.getKey("abcd");

    assertThat(clearCreationTime(receivedKey)).isEqualTo(key);
  }

  @Test
  public void getKey_missing() {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();

    // Verify an unchecked exception is thrown if a test attempts to fetch a key that hasn't been
    // explicitly stored.
    assertThrows(IllegalStateException.class, () -> keyDb.getKey("abcd"));
  }

  @Test
  public void getKey_setServiceException() {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();
    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));

    assertThrows(ServiceException.class, () -> keyDb.getKey("abcd"));
  }

  @Test
  public void createKey_successCreateKey() throws ServiceException {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();
    String keyId = "asdf";
    EncryptionKey expectedKey =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setJsonEncodedKeyset("12345")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    keyDb.createKey(expectedKey);
    EncryptionKey result = keyDb.getKey(keyId);

    assertThat(result).isEqualTo(expectedKey);
  }

  @Test
  public void createKey_exception() {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();
    keyDb.setServiceException(new ServiceException(INTERNAL, SERVICE_ERROR.name(), "error"));
    EncryptionKey key =
        EncryptionKey.newBuilder()
            .setKeyId("asdf")
            .setJsonEncodedKeyset("qwerty")
            .setCreationTime(0L)
            .setExpirationTime(0L)
            .build();

    assertThrows(ServiceException.class, () -> keyDb.createKey(key));
  }

  @Test
  public void getAllKeys_success() throws ServiceException {
    InMemoryKeyDb keyDb = new InMemoryKeyDb();

    IntStream.range(0, 10).forEach(unused -> createRandomKey(keyDb));

    assertThat(keyDb.getActiveKeys(5).size()).isEqualTo(5);
    assertThat(keyDb.getAllKeys().size()).isEqualTo(10);
  }

  private void createRandomKey(InMemoryKeyDb keyDb) {
    try {
      keyDb.createKey(FakeEncryptionKey.create());
    } catch (ServiceException e) {
      fail("Could not create key: " + e);
    }
  }
}
