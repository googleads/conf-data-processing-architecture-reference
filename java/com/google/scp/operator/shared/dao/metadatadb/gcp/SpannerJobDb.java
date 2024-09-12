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

import static com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerJobDb.SpannerJobTableColumn.JOB_ID_COLUMN;
import static com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerJobDb.SpannerJobTableColumn.TTL_COLUMN;
import static com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerJobDb.SpannerJobTableColumn.VALUE_COLUMN;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import com.google.cloud.Timestamp;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.ErrorCode;
import com.google.cloud.spanner.Mutation;
import com.google.cloud.spanner.Mutation.WriteBuilder;
import com.google.cloud.spanner.ResultSet;
import com.google.cloud.spanner.SpannerException;
import com.google.cloud.spanner.Statement;
import com.google.cloud.spanner.Value;
import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.common.collect.ImmutableList;
import com.google.inject.BindingAnnotation;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.time.Clock;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Optional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Spanner implementation of the {@code JobDb} */
public final class SpannerJobDb implements JobDb {

  private static final Logger logger = LoggerFactory.getLogger(SpannerJobDb.class);

  private static final JsonFormat.Printer JSON_PRINTER =
      JsonFormat.printer().includingDefaultValueFields();
  private static final JsonFormat.Parser JSON_PARSER = JsonFormat.parser().ignoringUnknownFields();
  private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper();

  private final DatabaseClient dbClient;
  private final String tableName;

  private final int ttlDays;

  /** Creates a new instance of the {@code SpannerDb} class. */
  @Inject
  SpannerJobDb(
      @JobDbClient DatabaseClient dbClient,
      @JobDbTableName String tableName,
      @JobDbSpannerTtlDays int ttlDays) {
    this.dbClient = dbClient;
    this.tableName = tableName;
    this.ttlDays = ttlDays;
  }

  @Override
  public Optional<Job> getJob(String jobId) throws JobDbException {
    try {
      Statement statement =
          Statement.newBuilder(
                  "SELECT * FROM " + tableName + " WHERE " + JOB_ID_COLUMN.label + " = @jobId")
              .bind("jobId")
              .to(jobId)
              .build();
      logger.debug("Executing spanner statement: " + statement);
      ResultSet resultSet = dbClient.singleUse().executeQuery(statement);

      // Expecting only one row to be returned since searching on primary key
      if (!resultSet.next()) {
        logger.debug("No job row found matches the job id: " + jobId);
        return Optional.empty();
      }
      return Optional.of(convertResultSetToJob(resultSet));
    } catch (SpannerException | InvalidProtocolBufferException | JsonProcessingException e) {
      throw new JobDbException(e);
    }
  }

  @Override
  public void putJob(Job job) throws JobDbException, JobIdExistsException {
    try {
      String jobInJson = convertJobToJsonString(job);
      Instant ttl = Clock.systemUTC().instant().plus(ttlDays, ChronoUnit.DAYS);
      WriteBuilder insertBuilder =
          Mutation.newInsertBuilder(tableName)
              .set(JOB_ID_COLUMN.label)
              .to(job.getJobId())
              .set(VALUE_COLUMN.label)
              .to(Value.json(jobInJson))
              .set(TTL_COLUMN.label)
              .to(
                  Timestamp.fromProto(
                      com.google.protobuf.Timestamp.newBuilder()
                          .setSeconds(ttl.getEpochSecond())
                          .build()));

      ImmutableList<Mutation> inserts = ImmutableList.of(insertBuilder.build());
      logger.debug("Executing spanner inserts: " + inserts);
      dbClient.write(inserts);
      logger.debug(String.format("Wrote job '%s' to spanner job metadata db.", job.getJobId()));
    } catch (SpannerException e) {
      if (e.getErrorCode() == ErrorCode.ALREADY_EXISTS) {
        throw new JobIdExistsException(e);
      } else {
        throw new JobDbException(e);
      }
    } catch (InvalidProtocolBufferException | JsonProcessingException e) {
      throw new JobDbException(e);
    }
  }

  private String convertJobToJsonString(Job job)
      throws InvalidProtocolBufferException, JsonProcessingException {
    JsonNode jobNode = OBJECT_MAPPER.readTree(JSON_PRINTER.print(job));
    // The job body is a JSON string and we store it as JSON node inside job.
    ((ObjectNode) jobNode).set("jobBody", OBJECT_MAPPER.readTree(job.getJobBody()));
    return OBJECT_MAPPER.writeValueAsString(jobNode);
  }

  private Job convertJsonStringToJob(String jobId, String jsonString)
      throws InvalidProtocolBufferException, JsonProcessingException {
    JsonNode jobNode = OBJECT_MAPPER.readTree(jsonString);

    JsonNode jobBodyNode = ((ObjectNode) jobNode).get("jobBody");
    String jobBody = OBJECT_MAPPER.writeValueAsString(jobBodyNode);

    Job.Builder jobBuilder = Job.newBuilder();
    // The job body is a JSON string. We remove it here so JSON_PARSER can convert JSON string to
    // job proto. It's value will be set after conversion.
    ((ObjectNode) jobNode).remove("jobBody");
    String jobAsJsonString = OBJECT_MAPPER.writeValueAsString(jobNode);
    JSON_PARSER.merge(jobAsJsonString, jobBuilder);

    return jobBuilder.setJobId(jobId).setJobBody(jobBody).build();
  }

  private Job convertResultSetToJob(ResultSet resultSet)
      throws InvalidProtocolBufferException, JsonProcessingException {
    String jobId = resultSet.getString(JOB_ID_COLUMN.label);
    String serializedValue = resultSet.getJson(VALUE_COLUMN.label);

    return convertJsonStringToJob(jobId, serializedValue);
  }

  /** Column names for the new Spanner job metadata table. */
  enum SpannerJobTableColumn {
    JOB_ID_COLUMN("JobId"),
    VALUE_COLUMN("Value"),
    TTL_COLUMN("Ttl");

    /** Value of a {@code SpannerJobTableColumn} constant. */
    public final String label;

    /** Constructor for setting {@code SpannerJobTableColumn} enum constants. */
    SpannerJobTableColumn(String label) {
      this.label = label;
    }
  }

  /** Annotation for the name of the job db table. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface JobDbTableName {}

  /** Annotation for the int of the TTL days to use for the metadata DB. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface JobDbSpannerTtlDays {}
}
