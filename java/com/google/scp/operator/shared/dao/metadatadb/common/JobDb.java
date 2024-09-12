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

package com.google.scp.operator.shared.dao.metadatadb.common;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.cmrt.sdk.job_service.v1.Job;
import com.google.inject.BindingAnnotation;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.util.Optional;

/** Interface for accessing new job metadata DB. */
public interface JobDb {

  /**
   * Retrieve job metadata for a given job id. Optional will be empty if no record exists.
   *
   * @param jobId the string representation of the job id
   * @throws JobDbException for other failures to read
   */
  Optional<Job> getJob(String jobId) throws JobDbException;

  /**
   * Insert a job metadata entry for a job, throwing an exception if the job id is already in use.
   *
   * @throws JobIdExistsException if the JobId is already in use by an item
   * @throws JobDbException for other failures to write
   */
  void putJob(Job job) throws JobDbException, JobIdExistsException;

  /** Represents an exception thrown by the {@code JobDb} class. */
  class JobDbException extends Exception {
    /** Creates a new instance of the {@code JobDbException} class. */
    public JobDbException(Throwable cause) {
      super(cause);
    }
  }

  /** Thrown if an insertion is attempted for a JobId that is already in use. */
  class JobIdExistsException extends Exception {
    /** Creates a new instance of the {@code JobIdExistsException} class. */
    public JobIdExistsException(Throwable cause) {
      super(cause);
    }

    /** Creates a new instance of the {@code JobIdExistsException} class with error message. */
    public JobIdExistsException(String message) {
      super(message);
    }
  }

  /** Annotation for the database client to use for the job DB. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  @interface JobDbClient {}
}
