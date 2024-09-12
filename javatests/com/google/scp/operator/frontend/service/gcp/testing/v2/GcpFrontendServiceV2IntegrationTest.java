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

package com.google.scp.operator.frontend.service.gcp.testing.v2;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.ACCEPTED;
import static com.google.scp.shared.api.model.Code.ALREADY_EXISTS;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.testutils.common.HttpRequestUtil.executeRequestWithRetry;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.acai.Acai;
import com.google.cmrt.sdk.job_service.v1.PutJobRequest;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.protos.shared.backend.jobqueue.JobQueueProto.JobQueueItem;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueException;
import com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueue;
import com.google.scp.operator.shared.dao.metadatadb.testing.HelloWorld;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.util.Optional;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Frontend integration test. Tests createJob and getJob requests. */
@RunWith(JUnit4.class)
public final class GcpFrontendServiceV2IntegrationTest {

  @Rule public Acai acai = new Acai(FrontendServiceV2IntegrationTestEnv.class);
  @Inject PubSubJobQueue pubSubJobQueue;
  @Inject private CloudFunctionEmulatorContainer functionContainer;

  private static final HttpClient client = HttpClient.newHttpClient();
  private static final String putJobPath = "/v1alpha/createJob";
  private static final String getJobPath = "/v1alpha/getJob";

  private static PutJobRequest CreatePutJobRequest() throws InvalidProtocolBufferException {
    HelloWorld helloworld = HelloWorld.newBuilder().setName("testname").setId(9773135).build();
    String jobBody = JsonFormat.printer().print(helloworld);
    return PutJobRequest.newBuilder().setJobId("55688").setJobBody(jobBody).build();
  }

  @Test
  public void testPutAndGetJob_success()
      throws IOException, JobQueueException, InvalidProtocolBufferException {
    PutJobRequest putJobRequest = CreatePutJobRequest();
    HttpRequest putRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(putJobPath))
            .setHeader("content-type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(getPutRequestJson(putJobRequest)))
            .build();
    HttpResponse<String> putJobResponse = executeRequestWithRetry(client, putRequest);

    Optional<JobQueueItem> item = pubSubJobQueue.receiveJob();

    HttpRequest getRequest =
        HttpRequest.newBuilder()
            .uri(
                getFunctionUri(
                    String.format("%s?job_request_id=%s", getJobPath, putJobRequest.getJobId())))
            .setHeader("content-type", "application/json")
            .GET()
            .build();
    HttpResponse<String> getJobResponse = executeRequestWithRetry(client, getRequest);
    JsonNode getJobNode = new ObjectMapper().readTree(getJobResponse.body());
    assertThat(putJobResponse.statusCode()).isEqualTo(ACCEPTED.getHttpStatusCode());
    assertThat(getJobResponse.statusCode()).isEqualTo(OK.getHttpStatusCode());
    assertThat(item.isPresent()).isTrue();
    assertThat(item.get().getJobKeyString()).isEqualTo(putJobRequest.getJobId());
    assertThat(
            putJobResponse.headers().map().get("content-type").get(0).contains("application/json"))
        .isTrue();
    assertThat(
            getJobResponse.headers().map().get("content-type").get(0).contains("application/json"))
        .isTrue();
    assertThat(getJobNode.get("job").get("job_id").asText()).isEqualTo(putJobRequest.getJobId());
    assertThat(getJobNode.get("job").get("server_job_id").asText()).isNotEqualTo("");
    JsonNode jobBodyNode = getJobNode.get("job").get("job_body");

    HelloWorld.Builder helloWorldBuilder = HelloWorld.newBuilder();
    JsonFormat.parser().merge(getJobNode.get("job").get("job_body").asText(), helloWorldBuilder);
    HelloWorld actualHelloWorld = helloWorldBuilder.build();
    assertThat(actualHelloWorld.getId()).isEqualTo(9773135);
    assertThat(actualHelloWorld.getName()).isEqualTo("testname");
    assertThat(getJobNode.get("job").get("job_status").asText()).isEqualTo("JOB_STATUS_CREATED");
    assertThat(getJobNode.get("job").get("retry_count").asInt()).isEqualTo(0);
  }

  private static String getPutRequestJson(PutJobRequest putJobRequest)
      throws InvalidProtocolBufferException {
    return JsonFormat.printer().print(putJobRequest);
  }

  @Test
  public void testPutJob_duplicateId() throws IOException, InterruptedException {
    HttpRequest putRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(putJobPath))
            .setHeader("Content-type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(getPutRequestJson(CreatePutJobRequest())))
            .build();
    HttpResponse<String> putJobResponse =
        client.send(putRequest, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));

    assertThat(putJobResponse.statusCode()).isEqualTo(ALREADY_EXISTS.getHttpStatusCode());
    assertThat(putJobResponse.body().contains("DUPLICATE_JOB_KEY")).isTrue();
  }

  @Test
  public void testGetJob_notFound() {
    HttpRequest getRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(String.format("%s?job_request_id=%s", getJobPath, "321")))
            .GET()
            .build();
    HttpResponse<String> getJobResponse = executeRequestWithRetry(client, getRequest);
    assertThat(getJobResponse.statusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());
    assertThat(getJobResponse.body().contains("JOB_NOT_FOUND")).isTrue();
  }

  private URI getFunctionUri(String path) {
    return URI.create("http://" + functionContainer.getEmulatorEndpoint() + path);
  }
}
