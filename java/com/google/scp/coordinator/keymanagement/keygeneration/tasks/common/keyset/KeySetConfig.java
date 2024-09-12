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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyset;

import com.google.auto.value.AutoValue;

/* Key set configuration. */
@AutoValue
public abstract class KeySetConfig {

  /**
   * Create an instance.
   *
   * @param name the name of the key set.
   * @param tinkTemplate the Tink template to be used for key generation.
   * @param count the number of keys created for each rotation.
   * @param validityInDays the validity of the keys in number of days.
   * @param ttlInDays the TTL of the keys in number of days.
   */
  public static KeySetConfig create(
      String name, String tinkTemplate, int count, int validityInDays, int ttlInDays) {
    return new AutoValue_KeySetConfig(name, tinkTemplate, count, validityInDays, ttlInDays);
  }

  /* Returns the name of the key set. */
  public abstract String getName();

  /* Returns the Tink template to be used for key generation. */
  public abstract String getTinkTemplate();

  /* Returns the number of keys created for each rotation. */
  public abstract int getCount();

  /* Returns the validity of the keys in number of days. */
  public abstract int getValidityInDays();

  /* Returns the TTL of the keys in number of days. */
  public abstract int getTtlInDays();
}
