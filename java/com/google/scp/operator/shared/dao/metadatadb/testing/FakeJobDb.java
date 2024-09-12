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

package com.google.scp.operator.shared.dao.metadatadb.testing;

import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb;
import java.util.Optional;

/** Fake implementation of the {@link JobDb} for use in tests. */
public final class FakeJobDb implements JobDb {

  // Values to return
  private Optional<Job> jobToReturn;

  // Last values requested with
  private String lastJobIdLookedUp;
  private Job lastJobPut;

  // Flags to throw exceptions
  private boolean shouldThrowJobDbException;
  private boolean shouldThrowJobIdExistsException;

  // Counts how many times a lookup has occurred for the same value consecutively
  private int jobLookupCount;

  // Number of times a GetJob call will fail before succeeding.
  private int initialLookupFailureCount = 0;

  /** Creates a new instance of the {@code FakeJobDb} class. */
  public FakeJobDb() {
    reset();
  }

  @Override
  public Optional<Job> getJob(String jobId) throws JobDbException {
    if (shouldThrowJobDbException) {
      throw new JobDbException(
          new IllegalStateException("Was set to throw (shouldThrowJobDbException)"));
    }
    if (initialLookupFailureCount > 0) {
      jobLookupCount++;
      if (lastJobIdLookedUp != null && lastJobIdLookedUp.equals(jobId)) {
        if (jobLookupCount > initialLookupFailureCount) {
          return jobToReturn;
        }
      } else {
        jobLookupCount = 1;
      }
      lastJobIdLookedUp = jobId;
      return Optional.empty();
    } else {
      lastJobIdLookedUp = jobId;
      return jobToReturn;
    }
  }

  @Override
  public void putJob(Job job) throws JobDbException, JobIdExistsException {
    if (shouldThrowJobDbException) {
      throw new JobDbException(
          new IllegalStateException("Was set to throw (shouldThrowJobDbException)"));
    }

    if (shouldThrowJobIdExistsException) {
      throw new JobIdExistsException(
          new IllegalStateException("Was set to throw (shouldThrowJobIdExistsException)"));
    }

    lastJobPut = job;
  }

  /** Set the job to be returned from the {@code getJob} method. */
  public void setJobToReturn(Optional<Job> jobToReturn) {
    this.jobToReturn = jobToReturn;
  }

  /** Set the number of job metadata lookups that have failed. */
  public void setInitialLookupFailureCount(int initialLookupFailureCount) {
    this.initialLookupFailureCount = initialLookupFailureCount;
  }

  /** Get the most recent job that was put. */
  public Job getLastJobPut() {
    return lastJobPut;
  }

  /**
   * Set if the {@code getJob} and {@code putJob} methods should throw the {@code jobDbException}.
   */
  public void setShouldThrowJobDbException(boolean shouldThrowJobDbException) {
    this.shouldThrowJobDbException = shouldThrowJobDbException;
  }

  /** Set if the {@code putJob} method should throw the {@code JobIdExistsException}. */
  public void setShouldThrowJobIdExistsException(boolean shouldThrowJobIdExistsException) {
    this.shouldThrowJobIdExistsException = shouldThrowJobIdExistsException;
  }

  /** Get the last job id used to try and retrieve job. */
  public String getLastJobIdLookedUp() {
    return lastJobIdLookedUp;
  }

  /** Sets all internal fields to their default values. */
  public void reset() {
    jobToReturn = Optional.empty();
    lastJobIdLookedUp = null;
    lastJobPut = null;
    shouldThrowJobDbException = false;
    shouldThrowJobIdExistsException = false;
    jobLookupCount = 0;
    initialLookupFailureCount = 0;
  }
}
