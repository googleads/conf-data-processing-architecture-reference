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

package com.google.scp.operator.frontend.tasks.testing;

import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.scp.operator.frontend.tasks.GetJobByIdTask;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.shared.api.exception.ServiceException;

/** Task to get a Job by Id. */
public final class FakeGetJobByIdTask implements GetJobByIdTask {

  /** Gets an existing job. */
  public Job getJobById(String jobId) throws ServiceException {
    return JobGenerator.createFakeJob(jobId);
  }
}
