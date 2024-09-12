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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.KeyIdFactory;

import static com.google.common.truth.Truth.assertThat;

import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdType;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.SequenceKeyIdFactory;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.UuidKeyIdFactory;
import java.util.Optional;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Test KeyIdType class. */
@RunWith(JUnit4.class)
public class KeyIdTypeTest {

  @Test
  public void testGetKeyIdFactory() {
    assertThat(KeyIdType.SEQUENCE.getKeyIdFactory().getClass())
        .isEqualTo(SequenceKeyIdFactory.class);
    assertThat(KeyIdType.UUID.getKeyIdFactory().getClass()).isEqualTo(UuidKeyIdFactory.class);
  }

  @Test
  public void getKeyIdTypeFromString() {
    assertThat(KeyIdType.fromString(Optional.of("UUID"))).isEqualTo(Optional.of(KeyIdType.UUID));
    assertThat(KeyIdType.fromString(Optional.of("SEQUENCE")))
        .isEqualTo(Optional.of(KeyIdType.SEQUENCE));
    assertThat(KeyIdType.fromString(Optional.empty())).isEqualTo(Optional.of(KeyIdType.UUID));
    assertThat(KeyIdType.fromString(Optional.of(""))).isEqualTo(Optional.empty());
    assertThat(KeyIdType.fromString(Optional.of("Random String"))).isEqualTo(Optional.empty());
  }
}
