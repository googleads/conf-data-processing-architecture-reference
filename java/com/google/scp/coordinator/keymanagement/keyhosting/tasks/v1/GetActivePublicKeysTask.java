/*
 * Copyright 2024 Google LLC
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

package com.google.scp.coordinator.keymanagement.keyhosting.tasks.v1;

import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter.EncodedPublicKeyListConverter;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter.EncodedPublicKeyListConverter.Mode;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ApiTask;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.RequestContext;
import com.google.scp.coordinator.keymanagement.shared.serverless.common.ResponseContext;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Service the <code>GetActivePublicKeys</code> endpoint. */
public class GetActivePublicKeysTask extends ApiTask {

  private final KeyDb keyDb;
  private final int keyLimit;

  @Inject
  GetActivePublicKeysTask(KeyDb keyDb, @KeyLimit Integer keyLimit) {
    super("GET", Pattern.compile("/sets/(?<name>[a-zA-Z0-9\\-]*)/publicKeys(?<raw>:raw)?"));
    this.keyDb = keyDb;
    this.keyLimit = keyLimit;
  }

  @Override
  protected void execute(Matcher matcher, RequestContext request, ResponseContext response)
      throws ServiceException {
    String setName = matcher.group("name");
    Mode mode = Mode.TINK;
    if (matcher.group("raw") != null) {
      mode = Mode.RAW;
    }
    ImmutableList<EncryptionKey> keys = keyDb.getActiveKeys(setName, keyLimit);
    EncodedPublicKeyListConverter converter = new EncodedPublicKeyListConverter(mode);
    response.setBody(GetActivePublicKeysResponse.newBuilder().addAllKeys(converter.convert(keys)));
  }
}
