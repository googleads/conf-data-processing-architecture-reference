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

import com.google.common.base.Converter;
import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;

public class CreateJobRequestWithMetadataToRequestInfoConverter
    extends Converter<CreateJobRequestWithMetadata, RequestInfo> {

  /** Converts the frontend CreateJobRequestWithMetadata model into the shared RequestInfo model. */
  @Override
  protected RequestInfo doForward(CreateJobRequestWithMetadata createJobRequestWithMetadata) {
    return RequestInfo.newBuilder()
        .setJobRequestId(createJobRequestWithMetadata.getCreateJobRequest().getJobRequestId())
        .setInputDataBucketName(
            createJobRequestWithMetadata.getCreateJobRequest().getInputDataBucketName())
        .setInputDataBlobPrefix(
            createJobRequestWithMetadata.getCreateJobRequest().getInputDataBlobPrefix())
        .setOutputDataBucketName(
            createJobRequestWithMetadata.getCreateJobRequest().getOutputDataBucketName())
        .setOutputDataBlobPrefix(
            createJobRequestWithMetadata.getCreateJobRequest().getOutputDataBlobPrefix())
        .setPostbackUrl(createJobRequestWithMetadata.getCreateJobRequest().getPostbackUrl())
        .setAccountIdentity(
            createJobRequestWithMetadata.getCreateJobRequestMetadata().getAccountIdentity())
        .putAllJobParameters(
            createJobRequestWithMetadata.getCreateJobRequest().getJobParametersMap())
        .build();
  }

  /** Converts the shared RequestInfo model into the frontend CreateJobRequestWithMetadata model. */
  @Override
  protected CreateJobRequestWithMetadata doBackward(RequestInfo requestInfo) {
    throw new UnsupportedOperationException();
  }
}
