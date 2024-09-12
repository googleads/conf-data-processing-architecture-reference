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

package com.google.scp.operator.shared.dao.metadatadb.gcp;

import static com.google.cmrt.sdk.job_service.v1.JobStatus.JOB_STATUS_CREATED;
import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static com.google.scp.shared.gcp.Constants.SPANNER_TEST_JOB_DB_NAME;
import static org.junit.Assert.assertThrows;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import com.google.acai.Acai;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.Key;
import com.google.cloud.spanner.Struct;
import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.inject.Inject;
import com.google.protobuf.Any;
import com.google.protobuf.TypeRegistry;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb.JobDbClient;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb.JobIdExistsException;
import com.google.scp.operator.shared.dao.metadatadb.testing.HelloWorld;
import com.google.scp.shared.proto.ProtoUtil;
import java.time.Instant;
import java.util.Collections;
import java.util.Optional;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class SpannerJobDbTest {
  @Rule public final Acai acai = new Acai(SpannerJobDbTestModule.class);

  @Inject SpannerJobDb spannerJobDb;
  @Inject @JobDbClient private DatabaseClient dbClient;

  private Job job;

  @Before
  public void setUp() throws Exception {
    String jobId = UUID.randomUUID().toString();
    HelloWorld helloworld = HelloWorld.newBuilder().setName("myname").setId(694324).build();
    Any anyMessage = Any.pack(helloworld);
    TypeRegistry typeRegistry = TypeRegistry.newBuilder().add(helloworld.getDescriptor()).build();
    String jobBody = JsonFormat.printer().usingTypeRegistry(typeRegistry).print(anyMessage);
    job =
        Job.newBuilder()
            .setJobId(jobId)
            .setServerJobId(UUID.randomUUID().toString())
            .setJobBody(jobBody)
            .setJobStatus(JOB_STATUS_CREATED)
            .setCreatedTime(ProtoUtil.toProtoTimestamp(Instant.parse("2023-01-01T00:00:00Z")))
            .setUpdatedTime(ProtoUtil.toProtoTimestamp(Instant.parse("2023-01-01T00:00:00Z")))
            .setRetryCount(0)
            .setProcessingStartedTime(
                ProtoUtil.toProtoTimestamp(Instant.parse("2023-01-01T00:00:00Z")))
            .build();
  }

  /** Test that getting a non-existent job item returns an empty optional */
  @Test
  public void getJobMetadata_getNonExistentReturnsEmpty() throws Exception {
    String nonexistentJobId = UUID.randomUUID().toString();

    Optional<Job> job = spannerJobDb.getJob(nonexistentJobId);
    assertThat(job).isEmpty();
  }

  /** Test that a job item can be put and read back */
  @Test
  public void putThenGet() throws Exception {
    spannerJobDb.putJob(job);
    Optional<Job> lookedUpJob = spannerJobDb.getJob(job.getJobId());

    Struct valueRow =
        dbClient
            .singleUse()
            .readRow(
                SPANNER_TEST_JOB_DB_NAME, Key.of(job.getJobId()), Collections.singleton("Value"));
    String valueString = valueRow.getJson("Value");

    String expectedValueString =
        "{\"createdTime\":\"2023-01-01T00:00:00Z\",\"jobBody\":{"
            + "\"@type\":\"type.googleapis.com/google.scp.operator.shared.dao.metadata.testing.HelloWorld\","
            + "\"id\":694324,\"name\":\"myname\"},\"jobId\":\""
            + job.getJobId()
            + "\","
            + "\"jobStatus\":\"JOB_STATUS_CREATED\","
            + "\"processingStartedTime\":\"2023-01-01T00:00:00Z\","
            + "\"retryCount\":0,"
            + "\"serverJobId\":\""
            + job.getServerJobId()
            + "\","
            + "\"updatedTime\":\"2023-01-01T00:00:00Z\"}";
    assertThat(valueString).isEqualTo(expectedValueString);

    assertThat(lookedUpJob).isPresent();
    assertThat(lookedUpJob.get().getJobId()).isEqualTo(job.getJobId());
    assertThat(lookedUpJob.get().getServerJobId()).isEqualTo(job.getServerJobId());
    assertThat(lookedUpJob.get().getJobStatus()).isEqualTo(job.getJobStatus());
    assertThat(lookedUpJob.get().getServerJobId()).isEqualTo(job.getServerJobId());
    assertThat(lookedUpJob.get().getCreatedTime()).isEqualTo(job.getCreatedTime());
    assertThat(lookedUpJob.get().getUpdatedTime()).isEqualTo(job.getUpdatedTime());
    assertThat(lookedUpJob.get().getRetryCount()).isEqualTo(job.getRetryCount());
    assertThat(lookedUpJob.get().getProcessingStartedTime())
        .isEqualTo(job.getProcessingStartedTime());

    JsonNode expectedJobBodyNode = new ObjectMapper().readTree(JsonFormat.printer().print(job));
    JsonNode lookedUpJobJobBodyNode =
        new ObjectMapper().readTree(JsonFormat.printer().print(lookedUpJob.get()));
    assertThat(((ObjectNode) lookedUpJobJobBodyNode).get("id"))
        .isEqualTo(((ObjectNode) expectedJobBodyNode).get("id"));
    assertThat(((ObjectNode) lookedUpJobJobBodyNode).get("name"))
        .isEqualTo(((ObjectNode) expectedJobBodyNode).get("name"));
  }

  /** Test that an exception is thrown when a duplicate job item is put */
  @Test
  public void putJob_putNonUniqueThrows() throws Exception {
    spannerJobDb.putJob(job);

    assertThrows(JobIdExistsException.class, () -> spannerJobDb.putJob(job));
  }
}
