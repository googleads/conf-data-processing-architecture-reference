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

package com.google.kms;

import static com.google.kms.LocalKmsConstants.DEFAULT_ENDPOINT;

import com.google.api.services.cloudkms.v1.model.DecryptRequest;
import com.google.api.services.cloudkms.v1.model.DecryptResponse;
import com.google.api.services.cloudkms.v1.model.EncryptRequest;
import com.google.api.services.cloudkms.v1.model.EncryptResponse;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AeadConfig;
import com.google.gson.Gson;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

/**
 * A local service built by HttpServer, simulating CloudKms service with Aead interface from Tink
 * library, providing encrypt and decrypt requests.
 */
public class LocalGcpKmsServer {
  public static void main(String[] args) throws Exception {
    // Initialize the FakeCloudKms with Aead
    AeadConfig.register();
    Aead aead = KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM")).getPrimitive(Aead.class);

    // Start service, and encrypt/decrypt endpoints
    HttpServer server = HttpServer.create(new InetSocketAddress(8000), 0);
    server.createContext(DEFAULT_ENDPOINT + "encrypt", new EncryptHandler(aead));
    server.createContext(DEFAULT_ENDPOINT + "decrypt", new DecryptHandler(aead));
    server.start();
  }

  // Handle key-uri:decrypt request
  static class DecryptHandler implements HttpHandler {
    Aead localAead;

    DecryptHandler(Aead aead) {
      localAead = aead;
    }

    @Override
    public void handle(HttpExchange exchange) throws IOException {
      // Read DecryptRequest from Http request body
      String requestBody = readAndUnzipRequest(exchange);

      // process the request and send the response
      parseRequestAndSendResponse(localAead, false, requestBody, exchange);
    }
  }

  // Handle key-uri:encrypt request
  static class EncryptHandler implements HttpHandler {
    Aead localAead;

    EncryptHandler(Aead aead) {
      localAead = aead;
    }

    @Override
    public void handle(HttpExchange exchange) throws IOException {
      // Read EncryptRequest from Http request body
      String requestBody = readAndUnzipRequest(exchange);

      // process the request and send the response
      parseRequestAndSendResponse(localAead, true, requestBody, exchange);
    }
  }

  private static String readAndUnzipRequest(HttpExchange exchange) throws IOException {
    GZIPInputStream gzipInputStream = new GZIPInputStream(exchange.getRequestBody());
    InputStreamReader inputReader = new InputStreamReader(gzipInputStream);
    BufferedReader bufferedReader = new BufferedReader(inputReader);
    StringBuilder requestReader = new StringBuilder();
    String line;
    while ((line = bufferedReader.readLine()) != null) {
      requestReader.append(line);
    }
    inputReader.close();
    bufferedReader.close();
    return requestReader.toString();
  }

  private static void parseRequestAndSendResponse(
      Aead localAead, boolean encrypt, String requestBody, HttpExchange exchange)
      throws IOException {
    String responseString;

    // Parse Json body to Request, and general response with Aead
    responseString =
        encrypt
            ? toEncryptionResponse(requestBody, localAead)
            : toDecryptionResponse(requestBody, localAead);

    // Send Encrypt/Decrypt Response through HTTP
    ByteArrayOutputStream gzipBytes = new ByteArrayOutputStream();
    GZIPOutputStream gzipOutputStream = new GZIPOutputStream(gzipBytes);
    gzipOutputStream.write(responseString.getBytes());
    gzipOutputStream.close();

    byte[] gzipResponse = gzipBytes.toByteArray();
    exchange.getResponseHeaders().set("Accept-encoding", "gzip");
    exchange.getResponseHeaders().set("Content-Encoding", "gzip");
    exchange.getResponseHeaders().set("Content-type", "application/json");
    exchange.sendResponseHeaders(200, gzipResponse.length);
    OutputStream responseBody = exchange.getResponseBody();
    InputStream compressedBodyInputStream = new ByteArrayInputStream(gzipResponse);
    byte[] buffer = new byte[1024];
    int byteRead;
    while ((byteRead = compressedBodyInputStream.read(buffer)) != -1) {
      responseBody.write(buffer, 0, byteRead);
    }
    responseBody.close();
  }

  private static String toEncryptionResponse(String requestBody, Aead localAead)
      throws IOException {
    Gson gson = new Gson();
    EncryptRequest encryptRequest = gson.fromJson(requestBody, EncryptRequest.class);
    EncryptResponse encryptResponse;
    try {
      byte[] ciphertext =
          localAead.encrypt(
              encryptRequest.decodePlaintext(), encryptRequest.decodeAdditionalAuthenticatedData());
      encryptResponse = new EncryptResponse().encodeCiphertext(ciphertext);
    } catch (Exception e) {
      throw new IOException(e.getMessage());
    }
    return gson.toJson(encryptResponse);
  }

  private static String toDecryptionResponse(String requestBody, Aead localAead)
      throws IOException {
    Gson gson = new Gson();
    DecryptRequest decryptRequest = gson.fromJson(requestBody, DecryptRequest.class);
    DecryptResponse decryptResponse;
    try {
      byte[] plaintext =
          localAead.decrypt(
              decryptRequest.decodeCiphertext(),
              decryptRequest.decodeAdditionalAuthenticatedData());
      if (plaintext.length == 0) {
        // The real CloudKMS also returns null in this case.
        decryptResponse = new DecryptResponse().encodePlaintext(null);
      } else {
        decryptResponse = new DecryptResponse().encodePlaintext(plaintext);
      }
    } catch (Exception e) {
      throw new IOException(e.getMessage());
    }
    return gson.toJson(decryptResponse);
  }
}
