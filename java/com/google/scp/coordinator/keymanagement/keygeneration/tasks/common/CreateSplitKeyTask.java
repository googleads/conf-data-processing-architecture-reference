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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common;

import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.util.KeyParams;
import java.time.Duration;
import java.time.Instant;

/** Interface for task classes that generate split keys */
public interface CreateSplitKeyTask {

  /**
   * Amount of time a key must be valid for to not be refreshed. Keys that expire before (now +
   * keyRefreshWindow) should be replaced with a new key.
   */
  Duration KEY_REFRESH_WINDOW = Duration.ofDays(1);

  /**
   * The actual key generation process. Performs the necessary key exchange key fetching (if
   * applicable), encryption key generation and splitting, key storage request, and database
   * persistence with signatures.
   *
   * <p>For key regeneration {@link CreateSplitKeyTask#replaceExpiringKeys(int, int, int)} should be
   * used.
   *
   * @param setName the name of the key set the keys belong to.
   * @param tinkTemplate the Tink template used for key generation.
   * @param count the number of keys is ensured to be active.
   * @param validityInDays the number of days each key should be active/valid for before expiring.
   * @param ttlInDays the number of days each key should be stored in the database.
   * @param activation the instant when the key should be active for encryption.
   */
  void createSplitKey(
      String setName,
      String tinkTemplate,
      int count,
      int validityInDays,
      int ttlInDays,
      Instant activation)
      throws ServiceException;

  /**
   * Same as {@link #createSplitKey(String, String, int, int, int, Instant)} except with {@link
   * KeyDb#DEFAULT_SET_NAME} as the key set name and {@link KeyParams#DEFAULT_TINK_TEMPLATE} as the
   * Tink template.
   *
   * @deprecated Use {@link #createSplitKey(String, String, int, int, int, Instant)} and specify the
   *     key set name and the Tink template.
   */
  @Deprecated
  default void createSplitKey(int count, int validityInDays, int ttlInDays, Instant activation)
      throws ServiceException {
    createSplitKey(
        KeyDb.DEFAULT_SET_NAME,
        KeyParams.DEFAULT_TINK_TEMPLATE,
        count,
        validityInDays,
        ttlInDays,
        activation);
  }

  /**
   * Counts the number of active keys in the KeyDB and creates enough keys to both replace any keys
   * expiring soon and replace any missing keys.
   *
   * <p>The generated keys will expire after validityInDays days, will have a Time-to-Live of
   * ttlInDays, and will be stored in the KeyDB.
   */
  void replaceExpiringKeys(int numDesiredKeys, int validityInDays, int ttlInDays)
      throws ServiceException;

  /**
   * Ensures {@code numDesiredKeys} active keys are currently available by creating new immediately
   * active keys to meet that number. For each active key, ensures that there is a corresponding
   * replacement key that will be active {@link #KEY_REFRESH_WINDOW} before the former expires.
   *
   * <p>The created immediately active keys expire in {@code validityInDays} days and will be in the
   * key database for {@code ttlInDays} days. The subsequent replacement keys will be active {@link
   * #KEY_REFRESH_WINDOW} before a currently active key expires, the replacement key will also
   * expire in {@code validityInDays} and in the key database for {@code ttlInDays} days.
   *
   * @param setName the name of the key set the keys belong to.
   * @param tinkTemplate the Tink template used for key generation.
   * @param numDesiredKeys the number of keys is ensured to be active.
   * @param validityInDays the number of days each key should be active/valid for before expiring.
   * @param ttlInDays the number of days each key should be stored in the database.
   */
  void create(
      String setName, String tinkTemplate, int numDesiredKeys, int validityInDays, int ttlInDays)
      throws ServiceException;

  /**
   * Same as {@link #create(String, String, int, int, int)} except with {@link
   * KeyDb#DEFAULT_SET_NAME} as the key set name and {@link KeyParams#DEFAULT_TINK_TEMPLATE} as the
   * Tink template.
   *
   * @deprecated Use {@link #create(String, String, int, int, int)} and specify the key set name.
   */
  @Deprecated
  default void create(int numDesiredKeys, int validityInDays, int ttlInDays)
      throws ServiceException {
    create(
        KeyDb.DEFAULT_SET_NAME,
        KeyParams.DEFAULT_TINK_TEMPLATE,
        numDesiredKeys,
        validityInDays,
        ttlInDays);
  }
}
