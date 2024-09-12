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

package com.google.scp.operator.frontend.service.gcp;

import static com.google.scp.shared.api.model.Code.ACCEPTED;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.HttpMethod.POST;
import static com.google.scp.shared.api.util.RequestUtil.validateHttpMethod;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProtoPreservingFieldNames;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.cmrt.sdk.job_service.v1.PutJobRequest;
import com.google.cmrt.sdk.job_service.v1.PutJobResponse;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.frontend.service.FrontendService;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandlerBase;
import java.io.IOException;
import java.util.stream.Collectors;

/** Handles requests to CreateJob Http Cloud Function and returns HTTP Response. */
public class PutJobRequestHandler
    extends CloudFunctionRequestHandlerBase<PutJobRequest, PutJobResponse> {

  private final FrontendService frontendService;

  /** Creates a new instance of the {@code CreateJobRequestHandler} class. */
  @Inject
  public PutJobRequestHandler(FrontendService frontendService) {
    this.frontendService = frontendService;
  }

  @Override
  protected PutJobRequest toRequest(HttpRequest httpRequest) throws ServiceException {
    try {
      validateHttpMethod(httpRequest.getMethod(), POST);
      String json = httpRequest.getReader().lines().collect(Collectors.joining());
      JsonFormat.Parser parser = JsonFormat.parser().ignoringUnknownFields();
      PutJobRequest.Builder builder = PutJobRequest.newBuilder();
      parser.merge(json, builder);
      return builder.build();
    } catch (IOException | RuntimeException exception) {
      throw new ServiceException(INVALID_ARGUMENT, ErrorReasons.JSON_ERROR.name(), exception);
    }
  }

  @Override
  protected PutJobResponse processRequest(PutJobRequest putJobRequest) throws ServiceException {
    return frontendService.putJob(putJobRequest);
  }

  @Override
  protected void toCloudFunctionResponse(HttpResponse httpResponse, PutJobResponse response)
      throws IOException {
    createCloudFunctionResponseFromProtoPreservingFieldNames(
        httpResponse, response, ACCEPTED.getHttpStatusCode(), allHeaders());
  }
}
