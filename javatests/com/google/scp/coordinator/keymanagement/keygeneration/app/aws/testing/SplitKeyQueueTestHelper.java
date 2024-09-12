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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing;

import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsUrl;
import static org.awaitility.Awaitility.await;

import com.google.inject.Inject;
import java.time.Duration;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sqs.model.GetQueueAttributesRequest;
import software.amazon.awssdk.services.sqs.model.QueueAttributeName;
import software.amazon.awssdk.services.sqs.model.SendMessageRequest;

/**
 * Test helper for interacting with a Key Generation SQS Queue in tests.
 *
 * @see SplitKeyGenerationStarter to create a local Key Generation service.
 */
public final class SplitKeyQueueTestHelper {
  private final SqsClient sqsClient;
  private final String queueUrl;

  @Inject
  public SplitKeyQueueTestHelper(SqsClient sqsClient, @KeyGenerationSqsUrl String queueUrl) {
    this.sqsClient = sqsClient;
    this.queueUrl = queueUrl;
  }

  /**
   * Inserts an item in the queue to trigger key generation.
   *
   * <p>Does not wait until the key generation process completes, use {@link waitForEmptyQueue} to
   * do this.
   */
  public void triggerKeyGeneration() {
    var sendMessageRequest =
        SendMessageRequest.builder().queueUrl(queueUrl).messageBody("").build();
    sqsClient.sendMessage(sendMessageRequest);
  }

  /**
   * Waits up to 120 seconds per item currently in the queue for the queue to no longer have any
   * items in it, indicating that all pending key generation tasks are complete.
   *
   * @throws org.awaitility.core.ConditionTimeoutException If condition was not fulfilled within the
   *     given time period.
   */
  public void waitForEmptyQueue() {
    // Note: KeyStorageService can take over 10 seconds per invocation so as a rule of thumb it's
    // probably good to wait 20 seconds per key that will be generated.  This hard coded 240 second
    // value means that we have time to process 5 keys and their 5 replacement keys.  If more than
    // ten keys are being generated, consider using {@code waitForEmptyQueue(Duration)}.

    var numMessages = countQueueMessages();
    if (numMessages == 0) {
      // awaitility will throw if told to wait 0 seconds, return early if queue is already flushed.
      return;
    }
    var duration = Duration.ofSeconds(360 * numMessages);
    waitForEmptyQueue(duration);
  }

  /**
   * Waits for up to duration for the queue to no longer have any items in it, indicating that all
   * pending key generation tasks are complete.
   *
   * @throws org.awaitility.core.ConditionTimeoutException If condition was not fulfilled within the
   *     given time period.
   */
  public void waitForEmptyQueue(Duration duration) {
    await()
        .atMost(new org.awaitility.Duration(duration.getSeconds(), TimeUnit.SECONDS))
        .until(() -> countQueueMessages() == 0);
  }

  /** Returns the number of items in the SQS queue */
  public int countQueueMessages() {
    List<QueueAttributeName> attributeNames =
        Arrays.asList(
            QueueAttributeName.APPROXIMATE_NUMBER_OF_MESSAGES,
            QueueAttributeName.APPROXIMATE_NUMBER_OF_MESSAGES_NOT_VISIBLE);
    var getQueueAttributesRequest =
        GetQueueAttributesRequest.builder()
            .queueUrl(queueUrl)
            .attributeNames(attributeNames)
            .build();
    var getQueueAttributesResponse = sqsClient.getQueueAttributes(getQueueAttributesRequest);
    Map<QueueAttributeName, String> responseMap = getQueueAttributesResponse.attributes();

    Integer visibleMessages =
        Integer.parseInt(responseMap.get(QueueAttributeName.APPROXIMATE_NUMBER_OF_MESSAGES));
    Integer nonvisibleMessages =
        Integer.parseInt(
            responseMap.get(QueueAttributeName.APPROXIMATE_NUMBER_OF_MESSAGES_NOT_VISIBLE));
    return visibleMessages + nonvisibleMessages;
  }
}
