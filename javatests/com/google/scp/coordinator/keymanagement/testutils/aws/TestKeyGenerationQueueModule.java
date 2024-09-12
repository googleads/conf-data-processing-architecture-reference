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

package com.google.scp.coordinator.keymanagement.testutils.aws;

import com.google.acai.AfterTest;
import com.google.acai.BeforeTest;
import com.google.acai.TestingService;
import com.google.acai.TestingServiceModule;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsUrl;
import java.util.UUID;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sqs.model.CreateQueueRequest;
import software.amazon.awssdk.services.sqs.model.DeleteQueueRequest;

/**
 * Small module which provides an KeyGenerationSqsUrl which is recreated for every test.
 *
 * <p>Intended to be used with LocalStack.
 */
public final class TestKeyGenerationQueueModule extends AbstractModule {

  // Randomize name to allow for use against a shared resource.
  public static final String QUEUE_NAME =
      "KeyGenerationIntegrationTest_" + UUID.randomUUID().toString();
  // Hardcode to what generated queue URL looks like for LocalStack.
  public static final String QUEUE_URL =
      String.format("http://localhost:4566/000000000000/%s", QUEUE_NAME);

  @Override
  public void configure() {
    install(TestingServiceModule.forServices(QueueCleanupService.class));
    bind(String.class).annotatedWith(KeyGenerationSqsUrl.class).toInstance(QUEUE_URL);
  }

  public static class QueueCleanupService implements TestingService {
    @Inject @KeyGenerationSqsUrl private String queueUrl;
    @Inject private SqsClient sqsClient;

    @BeforeTest
    void createQueue() {
      sqsClient.createQueue(CreateQueueRequest.builder().queueName(QUEUE_NAME).build());
    }

    @AfterTest
    void deleteQueue() {
      sqsClient.deleteQueue(DeleteQueueRequest.builder().queueUrl(queueUrl).build());
    }
  }
}
