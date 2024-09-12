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

package com.google.scp.operator.frontend.injection.modules.testing;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.auto.service.AutoService;
import com.google.common.base.Converter;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.inject.multibindings.Multibinder;
import com.google.scp.operator.frontend.injection.modules.BaseFrontendModule;
import com.google.scp.operator.frontend.serialization.JsonSerializerFacade;
import com.google.scp.operator.frontend.serialization.ObjectMapperSerializerFacade;
import com.google.scp.operator.frontend.service.FrontendService;
import com.google.scp.operator.frontend.service.FrontendServiceImpl;
import com.google.scp.operator.frontend.service.converter.CreateJobRequestWithMetadata;
import com.google.scp.operator.frontend.service.converter.CreateJobRequestWithMetadataToRequestInfoConverter;
import com.google.scp.operator.frontend.service.converter.ErrorCountConverter;
import com.google.scp.operator.frontend.service.converter.ErrorSummaryConverter;
import com.google.scp.operator.frontend.service.converter.GetJobResponseConverter;
import com.google.scp.operator.frontend.service.converter.JobStatusConverter;
import com.google.scp.operator.frontend.service.converter.ResultInfoConverter;
import com.google.scp.operator.frontend.tasks.CreateJobTask;
import com.google.scp.operator.frontend.tasks.GetJobByIdTask;
import com.google.scp.operator.frontend.tasks.PutJobTask;
import com.google.scp.operator.frontend.tasks.testing.FakeCreateJobTask;
import com.google.scp.operator.frontend.tasks.testing.FakeGetJobByIdTask;
import com.google.scp.operator.frontend.tasks.testing.FakePutJobTask;
import com.google.scp.operator.frontend.tasks.validation.RequestInfoValidator;
import com.google.scp.operator.frontend.testing.FakeRequestInfoValidator;
import com.google.scp.operator.protos.frontend.api.v1.ErrorCountProto;
import com.google.scp.operator.protos.frontend.api.v1.ErrorSummaryProto;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto;
import com.google.scp.operator.protos.frontend.api.v1.JobStatusProto;
import com.google.scp.operator.protos.frontend.api.v1.ResultInfoProto;
import com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount;
import com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary;
import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;
import com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue;
import com.google.scp.operator.shared.dao.jobqueue.testing.FakeJobQueue;
import com.google.scp.shared.mapper.TimeObjectMapper;

/** Contains all the bindings used for unit testing the frontend components. */
@AutoService(BaseFrontendModule.class)
public class FakeFrontendModule extends BaseFrontendModule {

  private static final FakeRequestInfoValidator fakeRequestInfoValidator;

  static {
    fakeRequestInfoValidator = new FakeRequestInfoValidator();
  }

  /** Provides an instance of the {@code FakeRequestInfoValidator} class. */
  @Provides
  @Singleton
  public static FakeRequestInfoValidator getFakeRequestInfoValidator() {
    return fakeRequestInfoValidator;
  }

  @Override
  public Class<? extends JsonSerializerFacade> getJsonSerializerImplementation() {
    return ObjectMapperSerializerFacade.class;
  }

  @Override
  public Class<? extends FrontendService> getFrontendServiceImplementation() {
    return FrontendServiceImpl.class;
  }

  @Override
  public Class<? extends Converter<JobMetadata, GetJobResponseProto.GetJobResponse>>
      getJobResponseConverterImplementation() {
    return GetJobResponseConverter.class;
  }

  @Override
  public Class<? extends Converter<CreateJobRequestWithMetadata, RequestInfo>>
      getCreateJobRequestWithMetadataToRequestInfoConverterImplementation() {
    return CreateJobRequestWithMetadataToRequestInfoConverter.class;
  }

  @Override
  public Class<? extends Converter<ResultInfo, ResultInfoProto.ResultInfo>>
      getResultInfoConverterImplementation() {
    return ResultInfoConverter.class;
  }

  @Override
  public Class<? extends Converter<ErrorSummary, ErrorSummaryProto.ErrorSummary>>
      getErrorSummaryConverterImplementation() {
    return ErrorSummaryConverter.class;
  }

  @Override
  public Class<
          ? extends
              Converter<
                  com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus,
                  JobStatusProto.JobStatus>>
      getJobStatusConverterImplementation() {
    return JobStatusConverter.class;
  }

  @Override
  public Class<? extends Converter<ErrorCount, ErrorCountProto.ErrorCount>>
      getErrorCountConverterImplementation() {
    return ErrorCountConverter.class;
  }

  @Override
  protected void configureModule() {
    bind(ObjectMapper.class).to(TimeObjectMapper.class);
    bind(JobQueue.class).to(FakeJobQueue.class);
    bindTaskModule();
    Multibinder<RequestInfoValidator> requestInfoValidatorMultibinder =
        Multibinder.newSetBinder(binder(), RequestInfoValidator.class);
    requestInfoValidatorMultibinder.addBinding().toInstance(fakeRequestInfoValidator);
  }

  public void bindTaskModule() {
    bind(CreateJobTask.class).to(FakeCreateJobTask.class);
    bind(PutJobTask.class).to(FakePutJobTask.class);
    bind(GetJobByIdTask.class).to(FakeGetJobByIdTask.class);
  }
}
