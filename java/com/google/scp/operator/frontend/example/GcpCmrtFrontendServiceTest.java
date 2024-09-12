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

package com.google.scp.operator.frontend.example;

import static com.google.common.truth.Truth.assertThat;
import static java.nio.charset.StandardCharsets.UTF_8;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.cmrt.cmrtworker.JobData;
import com.google.cmrt.sdk.job_service.v1.PutJobRequest;
import com.google.protobuf.util.JsonFormat;
import java.io.IOException;
import java.time.Duration;
import java.time.temporal.ChronoUnit;
import java.util.Optional;
import java.util.UUID;
import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.client.methods.RequestBuilder;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;

/** GCP CMRT FrontendService example */
public final class GcpCmrtFrontendServiceTest {
  private static final int TTL_DAYS = 365;
  private static final Duration COMPLETION_TIMEOUT_DEFAULT = Duration.of(5, ChronoUnit.MINUTES);
  private static final String PUBLIC_KEY = System.getenv().getOrDefault("PUBLIC_KEY", "public-key");
  private static final String DATA_BUCKET_NAME =
      System.getenv().getOrDefault("DATA_BUCKET", "frontendservice-testing");
  private static final String DATA_BLOB_NAME =
      System.getenv().getOrDefault("DATA_BLOB", "test_data_blob");
  private static final String RESULT_BUCKET_NAME =
      System.getenv().getOrDefault("RESULT_BUCKET", "frontendservice-testing");
  private static final String RESULT_BLOB_NAME =
      System.getenv().getOrDefault("RESULT_BLOB", "test_result_blob");
  private static final String ATTESTATION_INFO =
      System.getenv().getOrDefault("ATTESTATION_INFO", "");
  private static final String OWNER_ID = System.getenv().getOrDefault("OWNER_ID", "");
  private static final String TEST_DATA =
      System.getenv().getOrDefault("TEST_DATA", "some sensitive data");
  private static final String FRONTEND_CLOUDFUNCTION_URL =
      System.getenv("FRONTEND_CLOUDFUNCTION_URL");
  private static final String CREATE_JOB_URI_PATTERN = "%s/%s/createJob";
  private static final String GET_JOB_URI_PATTERN = "%s/%s/getJob?job_request_id=%s";
  private static final String API_GATEWAY_VERSION = "v1alpha";
  private static final String GCP_IDENTITY_TOKEN = System.getenv("GCP_IDENTITY_TOKEN");

  public static void main(String[] args) throws Exception {
    PutJobRequest putJobRequest = createPutJobRequest();
    submitPutJobRequest(putJobRequest);

    JsonNode result = getJobById(putJobRequest.getJobId());
    assertThat(result.get("job").get("job_status").asText()).isEqualTo("JOB_STATUS_CREATED");
    assertThat(result.get("job").get("retry_count").asInt()).isEqualTo(0);
  }

  private static PutJobRequest createPutJobRequest() {
    var jobData = createJobData();
    var jobBody = new String(jobData.toByteArray(), UTF_8);
    var jobId = UUID.randomUUID().toString();
    return PutJobRequest.newBuilder().setJobId(jobId).setJobBody(jobBody).build();
  }

  private static JobData createJobData() {
    return JobData.newBuilder()
        .setPublicKeyId(PUBLIC_KEY)
        .setDataBucketName(DATA_BUCKET_NAME)
        .setDataBlobName(DATA_BLOB_NAME)
        .setResultBucketName(RESULT_BUCKET_NAME)
        .setResultBlobName(RESULT_BLOB_NAME)
        .setOwnerId(OWNER_ID)
        .setAttestationInfo(ATTESTATION_INFO)
        .build();
  }

  private static void submitPutJobRequest(PutJobRequest putJobRequest)
      throws IOException, InterruptedException {
    String job = JsonFormat.printer().print(putJobRequest);
    String jobId = putJobRequest.getJobId();

    String createJobAPIUri = composeCreateJobUri();
    System.out.printf("createJobAPIUri: %s%n", createJobAPIUri);
    JsonNode createJobResult = createJob(createJobAPIUri, job);
    assertThat(createJobResult.get("job").get("job_id").asText()).isEqualTo(jobId);
    assertThat(createJobResult.get("job").get("job_status").asText())
        .isEqualTo("JOB_STATUS_CREATED");
  }

  private static String composeCreateJobUri() {
    return String.format(CREATE_JOB_URI_PATTERN, FRONTEND_CLOUDFUNCTION_URL, API_GATEWAY_VERSION);
  }

  private static JsonNode createJob(String uri, String requestBody) throws IOException {
    System.out.printf("callCreateJobAPI (%s) request: %s%n", uri, requestBody);
    JsonNode response = sendRequest(uri, "POST", Optional.ofNullable(requestBody));
    System.out.printf("callCreateJobAPI response: %s%n", response.toPrettyString());
    return response;
  }

  private static JsonNode getJobById(String jobId) throws Exception {
    String getJobAPIUri = composeGetJobUri(jobId);
    System.out.printf("getJobAPIUri: %s%n", getJobAPIUri);
    return getJob(getJobAPIUri);
  }

  private static String composeGetJobUri(String jobId) {
    return String.format(
        GET_JOB_URI_PATTERN, FRONTEND_CLOUDFUNCTION_URL, API_GATEWAY_VERSION, jobId);
  }

  private static JsonNode getJob(String uri) throws IOException {
    System.out.printf("callGetJobAPI (%s) request%n", uri);
    JsonNode response = sendRequest(uri, "GET", Optional.empty());
    System.out.printf("callGetJobAPI response: %s%n", response.toPrettyString());
    return response;
  }

  private static JsonNode sendRequest(String uri, String method, Optional<String> body)
      throws IOException {
    HttpClient client = HttpClients.custom().build();
    HttpResponse response = client.execute(composeRequest(uri, method, body));
    String content = EntityUtils.toString(response.getEntity());
    return new ObjectMapper().readTree(content);
  }

  private static HttpUriRequest composeRequest(String uri, String method, Optional<String> body)
      throws IOException {
    var builder = RequestBuilder.create(method);

    builder.setUri(uri);
    builder.addHeader("Authorization", String.format("Bearer %s", GCP_IDENTITY_TOKEN));
    if (body.isPresent()) {
      var entity = new StringEntity(body.get());
      builder.setEntity(entity);
    }
    return builder.build();
  }
}
