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

package com.google.scp.coordinator.keymanagement.keyhosting.tasks;

import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter.EncodedPublicKeyListConverter;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter.EncodedPublicKeyListConverter.Mode;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.CacheControlMaximum;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ApiTask;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Returns all active public keys */
public final class GetActivePublicKeysTask extends ApiTask {

  private final Logger logger = LoggerFactory.getLogger(GetActivePublicKeysTask.class);

  private static final String CORS_HEADER_NAME = "Access-Control-Allow-Origin";
  private static final String CACHE_CONTROL_HEADER_NAME = "Cache-Control";
  private static final String MAX_AGE_DIRECTIVE = "max-age=%s";

  private final KeyDb keyDb;
  private final int keyLimit;
  private final Long cacheControlMaximum;

  @Inject
  public GetActivePublicKeysTask(
      KeyDb keyDb, @KeyLimit Integer keyLimit, @CacheControlMaximum Long cacheControlMaximum) {
    super("GET", Pattern.compile("/publicKeys((?<raw>:raw)|(?<tink>:tink))?"));
    this.keyDb = keyDb;
    this.keyLimit = keyLimit;
    this.cacheControlMaximum = cacheControlMaximum;
  }

  /** Gets all public keys from the respective Key database */
  public ImmutableList<EncryptionKey> getActivePublicKeys() throws ServiceException {
    return keyDb.getActiveKeys(keyLimit);
  }

  @Override
  protected void execute(Matcher matcher, RequestContext request, ResponseContext response)
      throws ServiceException {
    Mode mode = Mode.LEGACY;
    if (matcher.group("tink") != null) {
      mode = Mode.TINK;
    } else if (matcher.group("raw") != null) {
      mode = Mode.RAW;
    }

    ImmutableList<EncryptionKey> keys = keyDb.getActiveKeys(KeyDb.DEFAULT_SET_NAME, keyLimit);
    if (keys.isEmpty()) {
      logger.error("No active public keys available.");
    }

    // Public keys should be accessible to any requesting code from any origin.
    response.addHeader(CORS_HEADER_NAME, "*");

    getSecondsUntilSoonestExpiration(keys)
        .ifPresent(
            seconds -> {
              response.addHeader(
                  CACHE_CONTROL_HEADER_NAME,
                  String.format(MAX_AGE_DIRECTIVE, Math.min(cacheControlMaximum, seconds)));
            });

    EncodedPublicKeyListConverter converter = new EncodedPublicKeyListConverter(mode);
    response.setBody(GetActivePublicKeysResponse.newBuilder().addAllKeys(converter.convert(keys)));
  }

  private static Optional<Long> getSecondsUntilSoonestExpiration(
      ImmutableList<EncryptionKey> keys) {
    return keys.stream()
        .map(EncryptionKey::getExpirationTime)
        .min(Long::compare)
        .map(soonest -> Instant.now().until(Instant.ofEpochMilli(soonest), ChronoUnit.SECONDS));
  }
}
