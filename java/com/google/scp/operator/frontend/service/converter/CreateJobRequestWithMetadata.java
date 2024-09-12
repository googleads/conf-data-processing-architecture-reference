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

package com.google.scp.operator.frontend.service.converter;

import com.google.auto.value.AutoValue;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestMetadataProto.CreateJobRequestMetadata;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;

/**
 * A helper class to wrap CreateJobRequests with any additional request metadata
 * (CreateJobRequestMetadata).
 */
@AutoValue
public abstract class CreateJobRequestWithMetadata {

  /** Returns a new instance of the {@code CreateJobRequestWithMetadata.Builder} class. */
  public static Builder builder() {
    return new AutoValue_CreateJobRequestWithMetadata.Builder();
  }

  /** Returns the CreateJobRequest. */
  public abstract CreateJobRequest getCreateJobRequest();

  /** Returns the CreateJobRequestMetadata. */
  public abstract CreateJobRequestMetadata getCreateJobRequestMetadata();

  /** Builder class for the {@code CreateJobRequestWithMetadata} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Set the CreateJobRequest. */
    public abstract Builder setCreateJobRequest(CreateJobRequest createJobRequest);

    /** Set the CreateJobRequestMetadata. */
    public abstract Builder setCreateJobRequestMetadata(
        CreateJobRequestMetadata createJobRequestMetadata);

    /**
     * Returns a new instance of the {@code CreateJobRequestWithMetadata} class from the builder.
     */
    public abstract CreateJobRequestWithMetadata build();
  }
}
