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

import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class KeySetsConfigTest {

  private static final ObjectMapper mapper = new ObjectMapper();

  @Test
  public void testMapper_noKeySets_readsExpected() throws Exception {
    // Given
    String json = "{\"key_sets\":[]}";

    // When
    KeySetsConfig configs = mapper.readValue(json, KeySetsConfig.class);

    // Then
    assertThat(configs).isEqualTo(KeySetsConfig.Builder.builder().keySets(List.of()).build());
  }

  @Test
  public void testMapper_hasKeySets_readsExpected() throws Exception {
    // Given
    String json =
        "{\"key_sets\":[{\"name\": \"set1\", \"tink_template\": \"my-template\"},{\"name\": \"set2\"}]}";

    // When
    KeySetsConfig configs = mapper.readValue(json, KeySetsConfig.class);

    // Then
    assertThat(configs)
        .isEqualTo(
            KeySetsConfig.Builder.builder()
                .keySets(
                    List.of(
                        KeySetsConfig.KeySet.Builder.builder()
                            .name("set1")
                            .tinkTemplate("my-template")
                            .build(),
                        KeySetsConfig.KeySet.Builder.builder().name("set2").build()))
                .build());
  }
}
