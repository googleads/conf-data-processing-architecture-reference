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

package com.google.scp.coordinator.keymanagement.shared.serverless.common;

import static com.google.scp.shared.api.util.ErrorUtil.toErrorResponse;

import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.MessageOrBuilder;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.shared.api.exception.ServiceException;
import java.io.UncheckedIOException;

/** Represents the HTTP response being returned to the client. */
public abstract class ResponseContext {

  private static final JsonFormat.Printer jsonPrinter =
      JsonFormat.printer().omittingInsignificantWhitespace().includingDefaultValueFields();

  /* Sets the response body. */
  public abstract void setBody(String body);

  /* Adds header to the response. */
  public abstract void addHeader(String name, String value);

  /* Sets the response status code. */
  public abstract void setStatusCode(int i);

  /* Sets the response body with the JSON representation of the protobuf message. */
  public void setBody(MessageOrBuilder message) {
    addHeader("Content-Type", "application/json");
    try {
      setBody(jsonPrinter.print(message));
    } catch (InvalidProtocolBufferException e) {
      throw new UncheckedIOException(e);
    }
  }

  /* Sets the response error code and body based on the exception. */
  public void setError(ServiceException exception) {
    setStatusCode(exception.getErrorCode().getHttpStatusCode());
    setBody(toErrorResponse(exception));
  }
}
