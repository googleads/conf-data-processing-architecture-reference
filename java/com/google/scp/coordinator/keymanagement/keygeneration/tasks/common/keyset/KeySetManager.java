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

import static com.google.common.collect.ImmutableList.toImmutableList;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.google.common.collect.ImmutableList;
import com.google.inject.BindingAnnotation;
import com.google.inject.Inject;
import com.google.inject.Provider;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationKeyCount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationTtlInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationValidityInDays;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.util.KeyParams;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.security.GeneralSecurityException;
import java.time.Duration;
import java.util.Optional;
import java.util.concurrent.TimeUnit;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Provides key sets management functionalities. */
public final class KeySetManager {

  private static final Logger logger = LoggerFactory.getLogger(KeySetManager.class);

  private static final Duration FETCHER_CACHE_DURATION = Duration.ofSeconds(20L);
  private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper();

  private final Supplier<Optional<KeySetsConfig>> configFetcher;
  private final int defaultCount;
  private final int defaultValidityInDays;
  private final int defaultTtlInDays;
  private final String defaultTemplate;
  private final Duration configCacheDuration;
  private final ImmutableList<KeySetConfig> defaultConfig;

  @Inject
  KeySetManager(
      @KeySetsJson Provider<Optional<String>> configProvider,
      @KeyGenerationKeyCount Integer count,
      @KeyGenerationValidityInDays Integer validityInDays,
      @KeyGenerationTtlInDays Integer ttlInDays,
      ConfigCacheDuration configCacheDuration)
      throws GeneralSecurityException {
    defaultCount = count;
    defaultValidityInDays = validityInDays;
    defaultTtlInDays = ttlInDays;
    defaultTemplate = KeyParams.DEFAULT_TINK_TEMPLATE;
    this.configCacheDuration = configCacheDuration.value;
    configFetcher = createFetcher(configProvider);
    defaultConfig =
        ImmutableList.of(
            KeySetConfig.create(
                KeyDb.DEFAULT_SET_NAME,
                defaultTemplate,
                defaultCount,
                defaultValidityInDays,
                defaultTtlInDays));
  }

  /** Returns all key set configurations. */
  public ImmutableList<KeySetConfig> getConfigs() {
    Optional<KeySetsConfig> config = configFetcher.get();
    if (config.isEmpty()) {
      return defaultConfig;
    }
    return config.get().keySets().stream()
        .map(
            keySet ->
                KeySetConfig.create(
                    keySet.name(),
                    keySet.tinkTemplate().orElse(defaultTemplate),
                    keySet.count().orElse(defaultCount),
                    keySet.validityInDays().orElse(defaultValidityInDays),
                    keySet.ttlInDays().orElse(defaultTtlInDays)))
        .collect(toImmutableList());
  }

  /**
   * Creates a cached fetcher fetch config every {@link #FETCHER_CACHE_DURATION} seconds. Remote
   * config is intended to take effect near-read-time and the cache is to throttle the rate.
   *
   * <p>Note ATTOW this does not work as intended yet due to the underlying provider backed by a
   * {@link com.google.scp.shared.clients.configclient.ParameterClient} with a much longer cache
   * duration such that re-fetching may still return stale config.
   */
  private Supplier<Optional<KeySetsConfig>> createFetcher(
      Provider<Optional<String>> configProvider) {
    return Suppliers.memoizeWithExpiration(
        () -> configProvider.get().map(KeySetManager::parseConfigJson),
        configCacheDuration.getSeconds(),
        TimeUnit.SECONDS);
  }

  private static KeySetsConfig parseConfigJson(String json) {
    try {
      return OBJECT_MAPPER.readValue(json, KeySetsConfig.class);
    } catch (JsonProcessingException e) {
      throw new IllegalArgumentException("Failed to parse key sets config.", e);
    }
  }

  /** Wrapper for overriding in tests. */
  static final class ConfigCacheDuration {
    Duration value = FETCHER_CACHE_DURATION;

    static ConfigCacheDuration of(Duration value) {
      ConfigCacheDuration instance = new ConfigCacheDuration();
      instance.value = value;
      return instance;
    }
  }

  /** Guice binding annotation for the key sets config JSON string. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface KeySetsJson {}
}
