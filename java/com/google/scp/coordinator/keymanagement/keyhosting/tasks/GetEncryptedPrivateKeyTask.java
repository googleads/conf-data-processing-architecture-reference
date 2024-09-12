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

import static com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter.EncryptionKeyConverter.toApiEncryptionKey;

import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ApiTask;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Performs the lookup for a specific private key. */
public final class GetEncryptedPrivateKeyTask extends ApiTask {
  private final KeyDb keyDb;

  @Inject
  public GetEncryptedPrivateKeyTask(KeyDb keyDb) {
    super("GET", Pattern.compile("/encryptionKeys/(?<id>[a-zA-Z0-9\\-]+)"));
    this.keyDb = keyDb;
  }

  /**
   * Gets a specific privateKey from the respective Key database using keyId. Does not return if key
   * not in Active status
   */
  public EncryptionKey getEncryptedPrivateKey(String keyId) throws ServiceException {
    return keyDb.getKey(keyId);
  }

  @Override
  protected void execute(Matcher matcher, RequestContext request, ResponseContext response)
      throws ServiceException {
    String id = matcher.group("id");
    response.setBody(toApiEncryptionKey(getEncryptedPrivateKey(id)));
  }
}
