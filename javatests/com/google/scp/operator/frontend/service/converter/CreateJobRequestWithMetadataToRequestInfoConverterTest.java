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

package com.google.scp.operator.frontend.service.converter;

import static com.google.common.truth.Truth.assertThat;

import com.google.scp.operator.frontend.service.ServiceJobGenerator;
import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class CreateJobRequestWithMetadataToRequestInfoConverterTest {

  private static final CreateJobRequestWithMetadata apiCreateJobRequestWithMetadata =
      ServiceJobGenerator.createFakeCreateJobRequestWithMetadataWithAccountIdentity("123");
  private static final RequestInfo requestInfo =
      JobGenerator.createFakeRequestInfoWithAccountIdentity("123");

  @Test
  public void doForward_success() {
    RequestInfo expected = requestInfo.toBuilder().build();

    CreateJobRequestWithMetadataToRequestInfoConverter converter =
        new CreateJobRequestWithMetadataToRequestInfoConverter();
    RequestInfo result = converter.convert(apiCreateJobRequestWithMetadata);

    assertThat(result).isEqualTo(expected);
  }
}
