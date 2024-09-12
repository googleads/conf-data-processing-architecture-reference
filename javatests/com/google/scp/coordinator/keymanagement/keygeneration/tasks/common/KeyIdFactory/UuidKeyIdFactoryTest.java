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

import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.UuidKeyIdFactory;
import java.util.UUID;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Test UuidKeyIdFactory. */
@RunWith(JUnit4.class)
public class UuidKeyIdFactoryTest {

  private final UuidKeyIdFactory keyIdFactory = new UuidKeyIdFactory();

  @Test
  public void testGetNextKeyId() {
    String generated = keyIdFactory.getNextKeyId(null);
    assertThat(UUID.fromString(generated).toString()).isEqualTo(generated);
  }
}
