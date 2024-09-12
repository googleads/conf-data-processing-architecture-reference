/*
 * Copyright 2023 Google LLC
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

import static com.google.common.collect.ImmutableList.toImmutableList;

import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter.EncryptionKeyConverter;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ApiTask;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.exception.SharedErrorReason;
import com.google.scp.shared.api.model.Code;
import java.time.Duration;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Stream;

/**
 * This task retrieves all recently created {@link EncryptionKey}s up to a specific age from {@link
 * KeyDb}.
 */
public final class ListRecentEncryptionKeysTask extends ApiTask {

  private static final String MAX_AGE_SECONDS_PARAM_NAME = "maxAgeSeconds";

  private final KeyDb keyDb;

  @Inject
  public ListRecentEncryptionKeysTask(KeyDb keyDb) {
    super("GET", Pattern.compile("/encryptionKeys:recent"));
    this.keyDb = keyDb;
  }

  /**
   * all recently created {@link EncryptionKey}s up to a specific age.
   *
   * @param maxAgeSeconds Maximum age of {@link EncryptionKey} that should be returned.
   */
  public Stream<EncryptionKey> execute(int maxAgeSeconds) throws ServiceException {
    if (maxAgeSeconds < 0) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          KeyManagementErrorReason.INVALID_ARGUMENT.name(),
          String.format(
              "%s should be positive, found (%s) instead.",
              MAX_AGE_SECONDS_PARAM_NAME, maxAgeSeconds));
    }
    return keyDb.listRecentKeys(Duration.ofSeconds(maxAgeSeconds));
  }

  @Override
  protected void execute(Matcher matcher, RequestContext request, ResponseContext response)
      throws ServiceException {
    Stream<EncryptionKey> keys = execute(getMaxAgeSeconds(request));
    response.setBody(
        ListRecentEncryptionKeysResponse.newBuilder()
            .addAllKeys(
                keys.map(EncryptionKeyConverter::toApiEncryptionKey).collect(toImmutableList())));
  }

  private static int getMaxAgeSeconds(RequestContext request) throws ServiceException {
    String maxAgeSeconds =
        request
            .getFirstQueryParameter(MAX_AGE_SECONDS_PARAM_NAME)
            .orElseThrow(
                () ->
                    new ServiceException(
                        Code.INVALID_ARGUMENT,
                        SharedErrorReason.INVALID_ARGUMENT.name(),
                        String.format(
                            "%s query parameter is required.", MAX_AGE_SECONDS_PARAM_NAME)));
    try {
      return Integer.parseInt(maxAgeSeconds);
    } catch (NumberFormatException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.name(),
          String.format("%s should be a valid integer.", MAX_AGE_SECONDS_PARAM_NAME),
          e);
    }
  }
}
