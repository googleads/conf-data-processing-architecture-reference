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

package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener;

import static com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.PubSubKeyGenerationTestModule.PROJECT_ID;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.PubSubKeyGenerationTestModule.SUBSCRIPTION_ID;
import static org.mockito.Mockito.verify;

import com.google.acai.Acai;
import com.google.cloud.pubsub.v1.Publisher;
import com.google.inject.Inject;
import com.google.protobuf.ByteString;
import com.google.pubsub.v1.PubsubMessage;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.ActuateKeySetTask;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.testutils.gcp.PubSubEmulatorContainer;
import java.util.Optional;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class CreateSplitKeysPubSubListenerTest {
  @Rule public final Acai acai = new Acai(PubSubKeyGenerationTestModule.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Inject PubSubEmulatorContainer pubSubEmulatorContainer;

  @Inject Publisher publisher;

  @Mock private ActuateKeySetTask mockSplitKeysTask;

  @Test
  public void start_success() throws ServiceException {
    int numberOfKeysToCreate = 5;
    int validityInDays = 8;
    int ttlInDays = 365;
    PubsubMessage message =
        PubsubMessage.newBuilder().setData(ByteString.copyFromUtf8("generateSplitKeys")).build();

    publisher.publish(message);

    PubSubListenerConfig config =
        PubSubListenerConfig.newBuilder()
            .setTimeoutInSeconds(Optional.of(5))
            .setEndpointUrl(Optional.of(pubSubEmulatorContainer.getEmulatorEndpoint()))
            .build();

    CreateSplitKeysPubSubListener listener =
        new CreateSplitKeysPubSubListener(
            config,
            mockSplitKeysTask,
            PROJECT_ID,
            SUBSCRIPTION_ID,
            numberOfKeysToCreate,
            validityInDays,
            ttlInDays);
    listener.start();

    verify(mockSplitKeysTask).execute();
  }
}
