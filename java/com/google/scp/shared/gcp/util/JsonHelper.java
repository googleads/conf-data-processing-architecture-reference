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

package com.google.scp.shared.gcp.util;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import io.cloudevents.CloudEvent;
import java.util.Base64;

public class JsonHelper {
  private static final String MESSAGE_FIELD_NAME = "message";
  private static final String DATA_FIELD_NAME = "data";
  private static final ObjectMapper jsonObjectMapper = new ObjectMapper();

  public static JsonNode parseMessagePublishedDataCloudEvent(CloudEvent event) {
    // Class com.google.events.cloud.pubsub.v1.MessagePublishedData
    var messagePublishedDataJson = parseJson(new String(event.getData().toBytes()));

    // Class com.google.events.cloud.pubsub.v1.Message
    var msgJson = getField(messagePublishedDataJson, MESSAGE_FIELD_NAME);
    var dataJson = getField(msgJson, DATA_FIELD_NAME);
    var decodedData = new String(Base64.getDecoder().decode(dataJson.asText()));

    return parseJson(decodedData);
  }

  public static JsonNode parseJson(String json) {
    try {
      return jsonObjectMapper.readTree(json);
    } catch (Exception e) {
      throw new RuntimeException("Failed to parse JSON string", e);
    }
  }

  public static JsonNode getField(JsonNode node, String field) {
    if (!node.has(field)) {
      throw new IllegalArgumentException(
          String.format("JSON payload %s does not contain the expected fields: %s", node, field));
    }
    if (node.get(field).isTextual() && node.get(field).asText().isEmpty()) {
      throw new IllegalArgumentException(
          String.format("JSON payload %s has empty field: %s", node, field));
    }
    return node.get(field);
  }

  public static JsonNode getFieldFirstEntry(JsonNode node, String field) {
    var arrayNode = getField(node, field);
    if (!arrayNode.isArray() || arrayNode.isEmpty()) {
      throw new IllegalArgumentException(String.format("Empty field %s in %s", field, node));
    }
    return arrayNode.get(0);
  }
}
