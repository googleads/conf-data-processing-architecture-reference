/*
 * Copyright 2024 Google LLC
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

package com.google.scp.operator.cpio.jobclient.model;

import com.google.auto.value.AutoValue;
import java.util.Optional;
import java.util.function.Function;
import javax.annotation.Nullable;

/** A class for defining parameters used to request getting a job. */
@AutoValue
public abstract class GetJobRequest {

  /** Returns a new instance of the {@code GetJobRequest.Builder} class. */
  public static Builder builder() {
    return new AutoValue_GetJobRequest.Builder();
  }

  /**
   * Returns the function used to get job completed notification topic id for the job. Users
   * maintains the mapping between job id and topic id.
   */
  @Nullable
  public abstract Optional<Function<Job, String>> getJobCompletionNotificationTopicIdFunc();

  /** Builder class for the {@code GetJobRequest} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Set the function used to get job completion notification topic id for the job. */
    public abstract Builder setJobCompletionNotificationTopicIdFunc(Function<Job, String> func);

    /** Returns a new instance of the {@code GetJobRequest} class from the builder. */
    public abstract GetJobRequest build();
  }
}
