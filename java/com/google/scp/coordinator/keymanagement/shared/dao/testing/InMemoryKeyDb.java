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

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.DATASTORE_ERROR;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDbUtil;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.time.Duration;
import java.time.Instant;
import java.util.HashMap;
import java.util.Map;
import java.util.stream.Stream;

/** In memory implementation of KeyDb for testing */
public final class InMemoryKeyDb implements KeyDb {

  private final Map<String, EncryptionKey> keys = new HashMap<>();
  private ServiceException serviceException;

  @Override
  public ImmutableList<EncryptionKey> getActiveKeys(String setName, int keyLimit, Instant instant)
      throws ServiceException {
    return getAllKeys().stream()
        .filter(k -> k.getSetName().equals(setName))
        .filter(k -> k.getExpirationTime() > instant.toEpochMilli())
        .filter(k -> k.getActivationTime() <= instant.toEpochMilli())
        .limit(keyLimit)
        .collect(toImmutableList());
  }

  @Override
  public EncryptionKey getKey(String keyId) throws ServiceException {
    if (serviceException != null) {
      throw serviceException;
    }
    EncryptionKey key = keys.get(keyId);

    if (key == null) {
      // Callers should use setServiceException instead to simulate missing value.
      throw new IllegalStateException("Missing private key from test db.");
    }

    return key;
  }

  @Override
  public void createKeys(ImmutableList<EncryptionKey> keys) throws ServiceException {
    if (serviceException != null) {
      throw serviceException;
    }
    // saves as many keys as possible.
    for (int i = 0; i < keys.size(); i++) {
      createKey(keys.get(i));
    }
  }

  /** Create key with overwrite option */
  @Override
  public void createKey(EncryptionKey key, boolean overwrite) throws ServiceException {
    if (serviceException != null) {
      throw serviceException;
    }

    if (!overwrite && keys.containsKey(key.getKeyId())) {
      throw new ServiceException(
          Code.ALREADY_EXISTS, DATASTORE_ERROR.name(), "KeyId already exists in the database");
    }
    keys.put(key.getKeyId(), key);
  }

  public void setServiceException(ServiceException serviceException) {
    this.serviceException = serviceException;
  }

  public void reset() {
    serviceException = null;
    keys.clear();
  }

  @Override
  public ImmutableList<EncryptionKey> getAllKeys() throws ServiceException {
    if (serviceException != null) {
      throw serviceException;
    }
    return keys.values().stream()
        .sorted(KeyDbUtil.getActiveKeysComparator())
        .collect(toImmutableList());
  }

  @Override
  public Stream<EncryptionKey> listRecentKeys(String setName, Duration maxAge)
      throws ServiceException {
    long maxCreationMilli = Instant.now().minus(maxAge).toEpochMilli();
    return getAllKeys().stream()
        .filter(k -> k.getSetName().equals(setName))
        .filter(k -> k.getCreationTime() >= maxCreationMilli);
  }
}
