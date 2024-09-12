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

import static com.google.scp.operator.frontend.tasks.ErrorMessages.DB_ERROR_MESSAGE;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.JOB_NOT_FOUND_MESSAGE;

import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.inject.Inject;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.frontend.tasks.GetJobByIdTask;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb;
import com.google.scp.operator.shared.dao.metadatadb.common.JobDb.JobDbException;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;

/** Task to get a Job by Id. */
public final class GcpGetJobByIdTask implements GetJobByIdTask {

  private final JobDb jobDb;

  /** Creates a new instance of the {@code GcpGetJobByIdTask} class. */
  @Inject
  public GcpGetJobByIdTask(JobDb jobDb) {
    this.jobDb = jobDb;
  }

  /** Gets an existing job. */
  public Job getJobById(String jobId) throws ServiceException {
    try {
      return jobDb
          .getJob(jobId)
          .orElseThrow(
              () ->
                  new ServiceException(
                      Code.NOT_FOUND,
                      ErrorReasons.JOB_NOT_FOUND.toString(),
                      String.format(JOB_NOT_FOUND_MESSAGE, jobId)));
    } catch (JobDbException e) {
      throw new ServiceException(
          Code.INTERNAL, ErrorReasons.SERVER_ERROR.toString(), DB_ERROR_MESSAGE, e);
    }
  }
}
