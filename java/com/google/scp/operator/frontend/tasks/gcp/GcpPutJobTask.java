/*
 * Copyright 2023 Google LLC
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

package com.google.scp.operator.frontend.tasks.gcp;

import static com.google.cmrt.sdk.job_service.v1.JobStatus.JOB_STATUS_CREATED;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.DB_ERROR_MESSAGE;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.DUPLICATE_JOB_MESSAGE;

import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.inject.Inject;
import com.google.protobuf.Timestamp;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.frontend.tasks.PutJobTask;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueException;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb.JobDbException;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb.JobIdExistsException;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.time.Clock;
import java.time.Instant;
import java.util.UUID;

/** Task to put a job in GCP. */
public final class GcpPutJobTask implements PutJobTask {

  private final JobDb jobDb;
  private final JobQueue jobQueue;
  private final Clock clock;

  /** Creates a new instance of the {@code GcpPutJobTask} class. */
  @Inject
  public GcpPutJobTask(JobDb jobDb, JobQueue jobQueue, Clock clock) {
    this.jobDb = jobDb;
    this.jobQueue = jobQueue;
    this.clock = clock;
  }

  public Job putJob(String jobId, String jobBody) throws ServiceException {
    Instant now = clock.instant();
    Timestamp currentTime = Timestamp.newBuilder().setSeconds(now.getEpochSecond()).build();

    String serverJobId = UUID.randomUUID().toString();
    Job job =
        Job.newBuilder()
            .setJobId(jobId)
            .setServerJobId(serverJobId)
            .setJobBody(jobBody)
            .setJobStatus(JOB_STATUS_CREATED)
            .setCreatedTime(currentTime)
            .setUpdatedTime(currentTime)
            .setRetryCount(0)
            .setProcessingStartedTime(Timestamp.newBuilder().build())
            .build();
    try {
      // Since we are enqueueing the job first, check if job already exists. Worker will handle edge
      // cases such as race condition by checking the server job id.
      if (jobDb.getJob(jobId).isPresent()) {
        throw new JobIdExistsException("Job already exists.");
      }

      // Create jobKey object as we share the jobQueue with V1.
      JobKey jobKey = JobKey.newBuilder().setJobRequestId(jobId).build();

      // It's important to enqueue the job first to make sure the job is processed
      jobQueue.sendJob(jobKey, serverJobId);
      jobDb.putJob(job);
      return job;
    } catch (JobDbException | JobQueueException e) {
      throw new ServiceException(
          Code.INTERNAL, ErrorReasons.SERVER_ERROR.toString(), DB_ERROR_MESSAGE, e);
    } catch (JobIdExistsException e) {
      throw new ServiceException(
          Code.ALREADY_EXISTS,
          ErrorReasons.DUPLICATE_JOB_KEY.toString(),
          String.format(DUPLICATE_JOB_MESSAGE, jobId));
    }
  }
}
