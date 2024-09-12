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

package com.google.scp.operator.frontend.service.gcp.testing.v2;

import com.google.inject.Guice;
import com.google.inject.Inject;
import com.google.inject.Injector;
import com.google.inject.Key;
import com.google.scp.operator.frontend.service.gcp.CreateJobRequestHandler;
import com.google.scp.operator.frontend.service.gcp.FrontendServiceHttpFunctionBase;
import com.google.scp.operator.frontend.service.gcp.GcpFrontendServiceModule.FrontendServiceVersionBinding;
import com.google.scp.operator.frontend.service.gcp.GetJobByIdRequestHandler;
import com.google.scp.operator.frontend.service.gcp.GetJobRequestHandler;
import com.google.scp.operator.frontend.service.gcp.PutJobRequestHandler;
import com.google.scp.operator.frontend.service.gcp.testing.FrontendServiceHttpFunctionTestModule;

/** Local http function for frontend service integration test */
public final class LocalFrontendServiceV2HttpFunction extends FrontendServiceHttpFunctionBase {
  public LocalFrontendServiceV2HttpFunction() {
    this(
        Guice.createInjector(
            new FrontendServiceHttpFunctionTestModule(), new FrontendServiceV2Module()));
  }

  @Inject
  public LocalFrontendServiceV2HttpFunction(Injector injector) {
    super(
        injector.getInstance(CreateJobRequestHandler.class),
        injector.getInstance(GetJobRequestHandler.class),
        injector.getInstance(PutJobRequestHandler.class),
        injector.getInstance(GetJobByIdRequestHandler.class),
        injector.getInstance(Key.get(Integer.class, FrontendServiceVersionBinding.class)));
  }
}
