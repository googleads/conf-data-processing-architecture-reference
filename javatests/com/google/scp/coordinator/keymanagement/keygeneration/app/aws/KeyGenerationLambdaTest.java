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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.amazonaws.services.lambda.runtime.events.ScheduledEvent;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateKeysTask;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

/**
 * Simple test for verifying KeyGenerationLambda triggers a CreateKeysTask. See {@link
 * KeyGenerationLambdaIntegrationTest} for a test that tests the behavior end to end.
 */
@RunWith(JUnit4.class)
public final class KeyGenerationLambdaTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private CreateKeysTask createKeysTask;

  @Test
  public void handleRequest_success() throws Exception {
    var keyGenerationKeyCount = 1;
    var keyGenerationValidityDays = 7;
    var keysTtlInDays = 365;
    var lambda =
        new KeyGenerationLambda(
            keyGenerationKeyCount, keyGenerationValidityDays, keysTtlInDays, createKeysTask);

    lambda.handleRequest(new ScheduledEvent(), null);

    verify(createKeysTask, times(1))
        .replaceExpiringKeys(keyGenerationKeyCount, keyGenerationValidityDays, keysTtlInDays);
  }

  @Test
  public void handleRequest_failure() throws Exception {
    doThrow(new ServiceException(Code.INTERNAL, "uh", "oh"))
        .when(createKeysTask)
        .replaceExpiringKeys(anyInt(), anyInt(), anyInt());

    var lambda = new KeyGenerationLambda(1, 7, 365, createKeysTask);

    assertThrows(
        IllegalStateException.class, () -> lambda.handleRequest(new ScheduledEvent(), null));
  }
}
