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

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class KeySetConfigTest {

  @Test
  public void testCreate_happyPath_returnsExpected() {
    // Given
    String setName = "test name";
    int count = 1111;
    int validityInDays = 2222;
    int ttlInDays = 3333;
    String tinkTemplate = "test template";

    // When
    KeySetConfig config =
        KeySetConfig.create(setName, tinkTemplate, count, validityInDays, ttlInDays);

    // Then
    assertThat(config.getName()).isEqualTo(setName);
    assertThat(config.getCount()).isEqualTo(count);
    assertThat(config.getValidityInDays()).isEqualTo(validityInDays);
    assertThat(config.getTtlInDays()).isEqualTo(ttlInDays);
    assertThat(config.getTinkTemplate()).isEqualTo(tinkTemplate);
  }
}
