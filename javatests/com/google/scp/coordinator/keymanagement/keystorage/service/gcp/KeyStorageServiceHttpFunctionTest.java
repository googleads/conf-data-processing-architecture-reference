/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.scp.shared.api.model.HttpMethod;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class KeyStorageServiceHttpFunctionTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;
  @Mock CreateKeyRequestHandler createKeyRequestHandler;

  private KeyStorageServiceHttpFunction cloudFunction;

  @Before
  public void setUp() throws IOException, NoSuchFieldException, IllegalAccessException {
    cloudFunction = new KeyStorageServiceHttpFunction(createKeyRequestHandler);
    StringWriter httpResponseOut = new StringWriter();
    BufferedWriter writerOut = new BufferedWriter(httpResponseOut);
    lenient().when(httpResponse.getWriter()).thenReturn(writerOut);
  }

  @Test
  public void service_createKeyApiSupported() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.POST.name());
    when(httpRequest.getPath()).thenReturn("/v1alpha/encryptionKeys");

    cloudFunction.service(httpRequest, httpResponse);

    verify(createKeyRequestHandler).handleRequest(httpRequest, httpResponse);
  }
}
