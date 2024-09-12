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
import static com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb.DEFAULT_SET_NAME;
import static com.google.scp.shared.util.KeyParams.DEFAULT_TINK_TEMPLATE;

import com.google.common.collect.ImmutableList;
import com.google.inject.util.Providers;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyset.KeySetManager.ConfigCacheDuration;
import java.time.Duration;
import java.util.Iterator;
import java.util.Optional;
import java.util.Random;
import java.util.stream.Stream;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class KeySetManagerTest {

  private static final int TEST_COUNT = new Random().nextInt();
  private static final int TEST_VALIDITY_IN_DAYS = new Random().nextInt();
  private static final int TEST_TTL_IN_DAYS = new Random().nextInt();
  private static final ConfigCacheDuration TEST_CACHE_DURATION =
      ConfigCacheDuration.of(Duration.ofSeconds(1));

  @Test
  public void testGetConfigs_noConfig_returnsOnlyDefaultConfig() throws Exception {
    // Given
    KeySetManager manager =
        new KeySetManager(
            Providers.of(Optional.empty()),
            TEST_COUNT,
            TEST_VALIDITY_IN_DAYS,
            TEST_TTL_IN_DAYS,
            new ConfigCacheDuration());

    // When
    ImmutableList<KeySetConfig> configs = manager.getConfigs();

    // Then
    assertThat(configs)
        .containsExactly(
            KeySetConfig.create(
                DEFAULT_SET_NAME,
                DEFAULT_TINK_TEMPLATE,
                TEST_COUNT,
                TEST_VALIDITY_IN_DAYS,
                TEST_TTL_IN_DAYS));
  }

  @Test
  public void testGetConfigs_nullConfigJson_returnsOnlyDefaultConfig() throws Exception {
    // Given
    String config = "null";
    KeySetManager manager =
        new KeySetManager(
            Providers.of(Optional.of(config)),
            TEST_COUNT,
            TEST_VALIDITY_IN_DAYS,
            TEST_TTL_IN_DAYS,
            new ConfigCacheDuration());

    // When
    ImmutableList<KeySetConfig> configs = manager.getConfigs();

    // Then
    assertThat(configs)
        .containsExactly(
            KeySetConfig.create(
                DEFAULT_SET_NAME,
                DEFAULT_TINK_TEMPLATE,
                TEST_COUNT,
                TEST_VALIDITY_IN_DAYS,
                TEST_TTL_IN_DAYS));
  }

  @Test
  public void testGetConfigs_explicitNoKeysets_emptyAndNoDefaultConfig() throws Exception {
    // Given
    String config = "{\"key_sets\":[]}";
    KeySetManager manager =
        new KeySetManager(
            Providers.of(Optional.of(config)),
            TEST_COUNT,
            TEST_VALIDITY_IN_DAYS,
            TEST_TTL_IN_DAYS,
            new ConfigCacheDuration());

    // When
    ImmutableList<KeySetConfig> configs = manager.getConfigs();

    // Then
    assertThat(configs).isEmpty();
  }

  @Test
  public void testGetConfigs_multipleKeySetsInConfig_returnsExpected() throws Exception {
    // Given
    String config =
        "{\"key_sets\":["
            + "{\"name\":\"set1\"},"
            + "{\"name\":\"set2\"},"
            + "{\"name\":\"set3\"}"
            + "]}";
    KeySetManager manager =
        new KeySetManager(
            Providers.of(Optional.of(config)),
            TEST_COUNT,
            TEST_VALIDITY_IN_DAYS,
            TEST_TTL_IN_DAYS,
            new ConfigCacheDuration());

    // When
    ImmutableList<KeySetConfig> configs = manager.getConfigs();

    // Then
    assertThat(configs)
        .containsExactly(
            KeySetConfig.create(
                "set1", DEFAULT_TINK_TEMPLATE, TEST_COUNT, TEST_VALIDITY_IN_DAYS, TEST_TTL_IN_DAYS),
            KeySetConfig.create(
                "set2", DEFAULT_TINK_TEMPLATE, TEST_COUNT, TEST_VALIDITY_IN_DAYS, TEST_TTL_IN_DAYS),
            KeySetConfig.create(
                "set3",
                DEFAULT_TINK_TEMPLATE,
                TEST_COUNT,
                TEST_VALIDITY_IN_DAYS,
                TEST_TTL_IN_DAYS));
  }

  @Test
  public void testGetConfigs_configChanged_returnsRefreshed() throws Exception {
    // Given
    Iterator<Optional<String>> jsons =
        Stream.of("{\"key_sets\":[{\"name\":\"v1\"}]}", "{\"key_sets\":[{\"name\":\"v2\"}]}")
            .map(Optional::of)
            .iterator();
    KeySetManager manager =
        new KeySetManager(
            jsons::next, TEST_COUNT, TEST_VALIDITY_IN_DAYS, TEST_TTL_IN_DAYS, TEST_CACHE_DURATION);

    // When
    ImmutableList<KeySetConfig> configs1 = manager.getConfigs();
    Thread.sleep(TEST_CACHE_DURATION.value.toMillis() + 500);
    ImmutableList<KeySetConfig> configs2 = manager.getConfigs();

    // Then
    assertThat(configs1).isNotEqualTo(configs2);
  }

  @Test
  public void testGetConfigs_configChanged_returnsUnexpiredCache() throws Exception {
    // Given
    Iterator<Optional<String>> jsons =
        Stream.of("{\"key_sets\":[{\"name\":\"v1\"}]}", "{\"key_sets\":[{\"name\":\"v2\"}]}")
            .map(Optional::of)
            .iterator();
    KeySetManager manager =
        new KeySetManager(
            jsons::next, TEST_COUNT, TEST_VALIDITY_IN_DAYS, TEST_TTL_IN_DAYS, TEST_CACHE_DURATION);

    // When
    ImmutableList<KeySetConfig> configs1 = manager.getConfigs();
    Thread.sleep(TEST_CACHE_DURATION.value.toMillis() - 500);
    ImmutableList<KeySetConfig> configs2 = manager.getConfigs();

    // Then
    assertThat(configs1).isEqualTo(configs2);
  }
}
