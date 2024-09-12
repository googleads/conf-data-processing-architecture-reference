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

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsUrl;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.SqsKeyGenerationQueue.SqsKeyGenerationQueueException;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.model.KeyGenerationQueueItem;
import java.time.Duration;
import java.util.Optional;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.core.exception.SdkException;
import software.amazon.awssdk.core.exception.SdkServiceException;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sqs.model.SendMessageRequest;

/** Hermetic Test for the SqsKeyGenerationQueue. */
@RunWith(JUnit4.class)
public class SqsKeyGenerationQueueTest {

  @Rule public Acai acai = new Acai(SqsKeyGenerationQueueTestEnv.class);

  @Inject @KeyGenerationSqsUrl private String queueUrl;
  @Inject private SqsClient sqsClient;

  // Under test
  @Inject private SqsKeyGenerationQueue sqsKeyGenerationQueue;

  /**
   * End to end test of queue functionality. Inserts, reads back, and acknowledges that the item was
   * processed.
   */
  @Test
  public void testInsertThenReadThenAck() throws Exception {
    insertIntoQueue();

    // Read the item back from the queue, check that it is the correct item
    Optional<KeyGenerationQueueItem> keyGenerationQueueItem = sqsKeyGenerationQueue.pullQueueItem();
    assertThat(keyGenerationQueueItem).isPresent();

    // Acknowledge processing of the item (deleting it from the queue)
    sqsKeyGenerationQueue.acknowledgeKeyGenerationCompletion(keyGenerationQueueItem.get());

    // Check that the queue is empty
    keyGenerationQueueItem = sqsKeyGenerationQueue.pullQueueItem();
    assertThat(keyGenerationQueueItem).isEmpty();
  }

  /** Test that an empty optional is returned if pullQueueItem is called on an empty queue */
  @Test
  public void testEmptyQueueYieldsEmptyReceive() throws Exception {
    Optional<KeyGenerationQueueItem> keyGenerationQueueItem = sqsKeyGenerationQueue.pullQueueItem();
    assertThat(keyGenerationQueueItem).isEmpty();
  }

  /** Test that an exception is thrown if a non-existent queue item is acknowledged */
  @Test
  public void testExceptionOnAckMissing() throws Exception {
    KeyGenerationQueueItem nonExistentItem =
        KeyGenerationQueueItem.builder()
            .setProcessingTimeout(Duration.ofSeconds(10))
            .setReceiptInfo("thisDoesntExist")
            .build();

    SqsKeyGenerationQueueException ex =
        assertThrows(
            SqsKeyGenerationQueueException.class,
            () -> sqsKeyGenerationQueue.acknowledgeKeyGenerationCompletion(nonExistentItem));
    assertThat(ex).hasMessageThat().isEqualTo("Error on acknowledgement of receipt");
    assertThat(ex).hasCauseThat().isInstanceOf(SdkServiceException.class);
    SdkServiceException sdkEx = (SdkServiceException) ex.getCause();
    assertThat(sdkEx.statusCode()).isEqualTo(400);
  }

  /** Insert a blank message into the queue for testing */
  private void insertIntoQueue() throws SdkException {
    SendMessageRequest sendMessageRequest =
        SendMessageRequest.builder().queueUrl(queueUrl).messageBody("").build();
    sqsClient.sendMessage(sendMessageRequest);
  }
}
