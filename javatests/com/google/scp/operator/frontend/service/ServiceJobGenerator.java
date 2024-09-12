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

package com.google.scp.operator.frontend.service;

import static com.google.cmrt.sdk.job_service.v1.JobStatus.JOB_STATUS_CREATED;
import static com.google.scp.operator.frontend.service.model.Constants.JOB_PARAM_ATTRIBUTION_REPORT_TO;
import static com.google.scp.operator.frontend.service.model.Constants.JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT;

import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.cmrt.sdk.job_service.v1.PutJobRequest;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.Timestamp;
import com.google.protobuf.util.Timestamps;
import com.google.scp.operator.frontend.service.converter.CreateJobRequestWithMetadata;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestMetadataProto.CreateJobRequestMetadata;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.frontend.api.v1.ErrorCountProto.ErrorCount;
import com.google.scp.operator.protos.frontend.api.v1.ErrorSummaryProto.ErrorSummary;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto.GetJobResponse;
import com.google.scp.operator.protos.frontend.api.v1.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.frontend.api.v1.ResultInfoProto.ResultInfo;
import com.google.scp.operator.protos.frontend.api.v1.ReturnCodeProto.ReturnCode;
import com.google.scp.operator.protos.shared.backend.JobErrorCategoryProto.JobErrorCategory;

/** Provides methods to generate fake frontend service model objects. */
public final class ServiceJobGenerator {

  private static final String DATA_HANDLE = "dataHandle";
  private static final String DATA_HANDLE_BUCKET = "bucket";
  private static final String POSTBACK_URL = "http://postback.com";
  private static final String ATTRIBUTION_REPORT_TO = "foo.com";
  private static final String DEBUG_PRIVACY_BUDGET_LIMIT = "5";
  private static final String ACCOUNT_IDENTITY = "service-account@testing.com";
  private static final String JOB_BODY = "jobBody";
  private static final Timestamp REQUEST_RECEIVED_AT =
      Timestamps.parseUnchecked("2019-10-01T08:25:24.00Z");
  private static final Timestamp REQUEST_UPDATED_AT =
      Timestamps.parseUnchecked("2019-10-01T08:29:24.00Z");
  private static final JobStatus JOB_STATUS_NEW_IMAGE = JobStatus.IN_PROGRESS;
  private static final ImmutableList<ErrorCount> ERROR_COUNTS =
      ImmutableList.of(
          ErrorCount.newBuilder()
              .setCategory(JobErrorCategory.DECRYPTION_ERROR.name())
              .setCount(5)
              .setDescription("Decryption error.")
              .build(),
          ErrorCount.newBuilder()
              .setCategory(JobErrorCategory.GENERAL_ERROR.name())
              .setDescription("General error.")
              .setCount(12)
              .build(),
          ErrorCount.newBuilder()
              .setCategory(JobErrorCategory.NUM_REPORTS_WITH_ERRORS.name())
              .setCount(17)
              .setDescription("Total number of reports with error.")
              .build());
  private static final ErrorSummary ERROR_SUMMARY =
      ErrorSummary.newBuilder().addAllErrorCounts(ERROR_COUNTS).build();
  private static final ResultInfo RESULT_INFO =
      ResultInfo.newBuilder()
          .setErrorSummary(ERROR_SUMMARY)
          .setFinishedAt(Timestamps.parseUnchecked("2019-10-01T13:25:24.00Z"))
          .setReturnCode(ReturnCode.SUCCESS.name())
          .setReturnMessage("Aggregation job successfully processed")
          .putAllResultMetadata(
              ImmutableMap.of("number of files processed", "30", "job duration", "4.5 hours"))
          .build();

  public static ResultInfo createFakeResultInfo() {
    return RESULT_INFO;
  }

  public static ErrorSummary CreateFakeErrorSummary() {
    return ERROR_SUMMARY;
  }

  public static ImmutableList<ErrorCount> createFakeErrorCounts() {
    return ERROR_COUNTS;
  }

  public static CreateJobRequest createFakeCreateJobRequest(String requestId) {
    return CreateJobRequest.newBuilder()
        .setJobRequestId(requestId)
        .setInputDataBlobPrefix(DATA_HANDLE)
        .setInputDataBucketName(DATA_HANDLE_BUCKET)
        .setOutputDataBlobPrefix(DATA_HANDLE)
        .setOutputDataBucketName(DATA_HANDLE_BUCKET)
        .setPostbackUrl(POSTBACK_URL)
        .putAllJobParameters(
            ImmutableMap.of(
                JOB_PARAM_ATTRIBUTION_REPORT_TO,
                ATTRIBUTION_REPORT_TO,
                JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT,
                DEBUG_PRIVACY_BUDGET_LIMIT))
        .build();
  }

  public static CreateJobRequestWithMetadata createFakeCreateJobRequestWithMetadata(
      String requestId) {
    return CreateJobRequestWithMetadata.builder()
        .setCreateJobRequest(createFakeCreateJobRequest(requestId).toBuilder().build())
        .setCreateJobRequestMetadata(CreateJobRequestMetadata.newBuilder().build())
        .build();
  }

  public static CreateJobRequestWithMetadata
      createFakeCreateJobRequestWithMetadataWithAccountIdentity(String requestId) {
    return CreateJobRequestWithMetadata.builder()
        .setCreateJobRequest(createFakeCreateJobRequest(requestId).toBuilder().build())
        .setCreateJobRequestMetadata(
            CreateJobRequestMetadata.newBuilder().setAccountIdentity(ACCOUNT_IDENTITY).build())
        .build();
  }

  public static GetJobResponse createFakeJobResponse(String requestId) {
    GetJobResponse getJobResponse =
        GetJobResponse.newBuilder()
            .setJobRequestId(requestId)
            .setInputDataBlobPrefix(DATA_HANDLE)
            .setInputDataBucketName(DATA_HANDLE_BUCKET)
            .setOutputDataBlobPrefix(DATA_HANDLE)
            .setOutputDataBucketName(DATA_HANDLE_BUCKET)
            .setPostbackUrl(POSTBACK_URL)
            .setJobStatus(JOB_STATUS_NEW_IMAGE)
            .setRequestReceivedAt(REQUEST_RECEIVED_AT)
            .setRequestUpdatedAt(REQUEST_UPDATED_AT)
            .setResultInfo(RESULT_INFO)
            .putAllJobParameters(
                ImmutableMap.of(
                    JOB_PARAM_ATTRIBUTION_REPORT_TO,
                    ATTRIBUTION_REPORT_TO,
                    JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT,
                    DEBUG_PRIVACY_BUDGET_LIMIT))
            .build();

    return getJobResponse;
  }

  public static PutJobRequest createFakePutJobRequest(String jobId) {
    return PutJobRequest.newBuilder().setJobId(jobId).setJobBody(JOB_BODY).build();
  }

  public static Job createFakeJob(String jobId, String serverJobId) {
    return Job.newBuilder()
        .setJobId(jobId)
        .setServerJobId(serverJobId)
        .setJobBody(JOB_BODY)
        .setJobStatus(JOB_STATUS_CREATED)
        .setCreatedTime(REQUEST_RECEIVED_AT)
        .setUpdatedTime(REQUEST_UPDATED_AT)
        .setRetryCount(0)
        .build();
  }
}
