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

package com.google.scp.operator.frontend.tasks.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.DB_ERROR_MESSAGE;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.DUPLICATE_JOB_MESSAGE;
import static com.google.scp.shared.api.exception.testing.ServiceExceptionAssertions.assertThatServiceExceptionMatches;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.acai.TestScoped;
import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Singleton;
import com.google.protobuf.Timestamp;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.frontend.tasks.PutJobTask;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue;
import com.google.scp.operator.shared.dao.jobqueue.testing.FakeJobQueue;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.FakeJobDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.operator.shared.testing.FakeClock;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.proto.ProtoUtil;
import java.time.Clock;
import java.time.Instant;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class GcpPutJobTaskTest {

  static final String jobId = "5678";
  static final String jobBody = "TestBody";
  @Rule public Acai acai = new Acai(GcpPutJobTaskTest.TestEnv.class);

  PutJobTask putJobTask;
  @Inject JobQueue jobQueue;
  @Inject JobDb jobDb;
  @Inject Clock clock;

  Job job;
  FakeJobDb fakeJobDb;
  FakeJobQueue fakeJobQueue;
  Instant clockTime;

  @Before
  public void setup() {
    putJobTask = new GcpPutJobTask(jobDb, jobQueue, clock);
    clockTime = clock.instant();
    job =
        JobGenerator.createFakeJob(jobId).toBuilder()
            .setCreatedTime(ProtoUtil.toProtoTimestamp(clockTime))
            .setUpdatedTime(ProtoUtil.toProtoTimestamp(clockTime))
            .setProcessingStartedTime(Timestamp.newBuilder().build())
            .build();
    fakeJobDb = (FakeJobDb) jobDb;
    fakeJobQueue = (FakeJobQueue) jobQueue;
    fakeJobDb.reset();
  }

  /** Test for scenario to put a job with no failures */
  @Test
  public void putJob_putsJobInDb() throws Exception {
    putJobTask.putJob(jobId, jobBody);
    Job createdJob = fakeJobDb.getLastJobPut();
    assertThat(createdJob)
        .isEqualTo(
            job.toBuilder()
                .setServerJobId(createdJob.getServerJobId())
                .setJobBody(jobBody)
                .build());
    assertThat(fakeJobQueue.getLastJobKeySent().getJobRequestId()).isEqualTo(jobId);
  }

  /** Test for scenario to put a job where the JobKey is taken */
  @Test
  public void putJob_throwsException_whenJobIdIsDuplicateInDb() {
    fakeJobDb.setShouldThrowJobIdExistsException(true);

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> putJobTask.putJob(jobId, jobBody));

    ServiceException expectedServiceException =
        new ServiceException(
            Code.ALREADY_EXISTS,
            ErrorReasons.DUPLICATE_JOB_KEY.toString(),
            String.format(DUPLICATE_JOB_MESSAGE, jobId));
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);

    // Verify that only the queue has a job entry
    assertThat(fakeJobDb.getLastJobPut()).isNull();
    assertThat(fakeJobQueue.getLastJobKeySent().getJobRequestId()).isEqualTo(jobId);
  }

  /** Test for scenario to put a job with some internal error in db */
  @Test
  public void putJob_throwsException_whenDbThrowsException() {
    fakeJobDb.setShouldThrowJobDbException(true);

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> putJobTask.putJob(jobId, jobBody));

    ServiceException expectedServiceException =
        new ServiceException(Code.INTERNAL, ErrorReasons.SERVER_ERROR.toString(), DB_ERROR_MESSAGE);
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);

    // The initial check for job will fail so nothing is queued or put
    assertThat(fakeJobDb.getLastJobPut()).isNull();
    assertThat(fakeJobQueue.getLastJobKeySent()).isNull();
  }

  /** Test for scenario to insert a job with some internal error in queue */
  @Test
  public void createJob_throwsException_whenQueueThrowsException() {
    fakeJobQueue.setShouldThrowException(true);

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> putJobTask.putJob(jobId, jobBody));

    ServiceException expectedServiceException =
        new ServiceException(Code.INTERNAL, ErrorReasons.SERVER_ERROR.toString(), DB_ERROR_MESSAGE);
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);

    // Verify that queue and db never received a job
    assertThat(fakeJobDb.getLastJobPut()).isNull();
    assertThat(fakeJobQueue.getLastJobKeySent()).isNull();
  }

  static class TestEnv extends AbstractModule {
    @Override
    public void configure() {
      bind(Clock.class).to(FakeClock.class).in(Singleton.class);
      bind(JobQueue.class).to(FakeJobQueue.class).in(TestScoped.class);
      bind(JobDb.class).to(FakeJobDb.class).in(TestScoped.class);
    }
  }
}
