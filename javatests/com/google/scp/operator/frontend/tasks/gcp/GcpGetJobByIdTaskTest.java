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
import static com.google.scp.operator.frontend.tasks.ErrorMessages.JOB_NOT_FOUND_MESSAGE;
import static com.google.scp.shared.api.exception.testing.ServiceExceptionAssertions.assertThatServiceExceptionMatches;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.acai.TestScoped;
import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Singleton;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.frontend.tasks.GetJobByIdTask;
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
import java.util.Optional;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class GcpGetJobByIdTaskTest {

  static final String jobId = "5788";

  @Rule public Acai acai = new Acai(GcpGetJobByIdTaskTest.TestEnv.class);
  // Under test
  GetJobByIdTask getJobByIdTask;
  @Inject FakeJobDb fakeJobDb;
  @Inject Clock clock;

  Job job;
  Instant clockTime;

  @Before
  public void setUp() {
    getJobByIdTask = new GcpGetJobByIdTask(fakeJobDb);
    clockTime = clock.instant();
    job =
        JobGenerator.createFakeJob(jobId).toBuilder()
            .setCreatedTime(ProtoUtil.toProtoTimestamp(clockTime))
            .setUpdatedTime(ProtoUtil.toProtoTimestamp(clockTime))
            .setProcessingStartedTime(ProtoUtil.toProtoTimestamp(clockTime))
            .build();
    fakeJobDb.reset();
  }

  /** Test for scenario to get a job by id that exists with no failures */
  @Test
  public void getJobById_returnsJob() throws Exception {
    fakeJobDb.setJobToReturn(Optional.of(job));

    Job actualJob = getJobByIdTask.getJobById(jobId);

    assertThat(actualJob).isEqualTo(job);
    assertThat(fakeJobDb.getLastJobIdLookedUp()).isEqualTo(jobId);
  }

  /** Test for scenario to get a job by id that does not exist with no failures */
  @Test
  public void getJobById_throwsException_whenJobDoesNotExist() {
    fakeJobDb.setJobToReturn(Optional.empty());

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> getJobByIdTask.getJobById(jobId));

    ServiceException expectedServiceException =
        new ServiceException(
            Code.NOT_FOUND,
            ErrorReasons.JOB_NOT_FOUND.toString(),
            String.format(JOB_NOT_FOUND_MESSAGE, jobId));
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);
    assertThat(fakeJobDb.getLastJobIdLookedUp()).isEqualTo(jobId);
  }

  /** Test for scenario to get a job with some internal error happening */
  @Test
  public void getJobById_throwsException_whenDbThrowsException() {
    fakeJobDb.setShouldThrowJobDbException(true);

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> getJobByIdTask.getJobById(jobId));

    ServiceException expectedServiceException =
        new ServiceException(Code.INTERNAL, ErrorReasons.SERVER_ERROR.toString(), DB_ERROR_MESSAGE);
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);
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
