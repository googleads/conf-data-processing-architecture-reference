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

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.google.auto.value.AutoValue;
import java.util.List;
import java.util.Optional;

/**
 * This class is the key set configuration model defining the schema for the JSON deserialization.
 */
@AutoValue
@JsonDeserialize(builder = KeySetsConfig.Builder.class)
abstract class KeySetsConfig {

  @JsonProperty("key_sets")
  abstract List<KeySet> keySets();

  @AutoValue.Builder
  abstract static class Builder {

    @JsonProperty("key_sets")
    abstract KeySetsConfig.Builder keySets(List<KeySet> keySets);

    abstract KeySetsConfig build();

    @JsonCreator
    static KeySetsConfig.Builder builder() {
      return new AutoValue_KeySetsConfig.Builder();
    }
  }

  @AutoValue
  @JsonDeserialize(builder = KeySet.Builder.class)
  abstract static class KeySet {

    @JsonProperty("name")
    abstract String name();

    abstract Optional<Integer> count();

    abstract Optional<Integer> validityInDays();

    abstract Optional<Integer> ttlInDays();

    abstract Optional<String> tinkTemplate();

    @JsonIgnoreProperties(ignoreUnknown = true)
    @AutoValue.Builder
    abstract static class Builder {

      @JsonProperty("name")
      abstract Builder name(String name);

      abstract Builder count(int count);

      abstract Builder validityInDays(int validityInDays);

      abstract Builder ttlInDays(int ttlInDays);

      @JsonProperty("tink_template")
      Builder tinkTemplate(String tinkTemplate) {
        return tinkTemplate(Optional.ofNullable(tinkTemplate));
      }

      abstract Builder tinkTemplate(Optional<String> tinkTemplate);

      abstract KeySet build();

      @JsonCreator
      static KeySet.Builder builder() {
        return new AutoValue_KeySetsConfig_KeySet.Builder();
      }
    }
  }
}
