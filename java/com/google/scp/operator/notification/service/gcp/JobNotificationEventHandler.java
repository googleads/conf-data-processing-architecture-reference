/*
 * Copyright 2024 Google LLC
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

package com.google.scp.operator.notification.service.gcp;

import static com.google.scp.shared.gcp.util.JsonHelper.getField;
import static com.google.scp.shared.gcp.util.JsonHelper.parseMessagePublishedDataCloudEvent;

import com.fasterxml.jackson.databind.JsonNode;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.operator.cpio.notificationclient.NotificationClient;
import com.google.scp.operator.cpio.notificationclient.NotificationClient.NotificationClientException;
import com.google.scp.operator.cpio.notificationclient.gcp.GcpNotificationClientModule;
import com.google.scp.operator.cpio.notificationclient.model.PublishMessageRequest;
import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.shared.backend.jobnotification.JobNotificationEvent;
import com.google.scp.shared.gcp.util.CloudEventFunctionHandlerBase;
import io.cloudevents.CloudEvent;
import org.slf4j.Logger;

/** This class will receive events triggered by the completion of TEE jobs. */
public class JobNotificationEventHandler
    extends CloudEventFunctionHandlerBase<JobNotificationEvent> {

  private static final Logger logger = getLogger(JobNotificationEventHandler.class);

  private static final String JOB_ID_FIELD_NAME = "jobId";
  private static final String JOB_STATUS_FIELD_NAME = "jobStatus";
  private static final String TOPIC_ID_FIELD_NAME = "topicId";

  private final NotificationClient notificationClient;

  public JobNotificationEventHandler() {
    this(Guice.createInjector(new GcpNotificationClientModule()));
  }

  JobNotificationEventHandler(Injector injector) {
    this(injector.getInstance(NotificationClient.class));
  }

  JobNotificationEventHandler(NotificationClient notificationClient) {
    this.notificationClient = notificationClient;
  }

  @Override
  protected JobNotificationEvent toEvent(CloudEvent cloudEvent) throws Exception {
    JsonNode jsonNode = parseMessagePublishedDataCloudEvent(cloudEvent);
    return parseJsonNodeToJobNotificationEvent(jsonNode);
  }

  @Override
  protected void handleEvent(JobNotificationEvent event) throws NotificationClientException {
    String messageBody = String.format("{\"jobId\": \"%s\"}", event.getJobId());
    PublishMessageRequest publishMessageRequest =
        PublishMessageRequest.builder()
            .setNotificationTopic(event.getTopicId())
            .setMessageBody(messageBody)
            .build();
    notificationClient.publishMessage(publishMessageRequest);
  }

  private JobNotificationEvent parseJsonNodeToJobNotificationEvent(JsonNode jsonNode)
      throws Exception {
    String jobId = getField(jsonNode, JOB_ID_FIELD_NAME).asText();
    String topicId = getField(jsonNode, TOPIC_ID_FIELD_NAME).asText();
    var event = JobNotificationEvent.newBuilder().setJobId(jobId).setTopicId(topicId);
    String jobStatus = getField(jsonNode, JOB_STATUS_FIELD_NAME).asText();
    if (jobStatus.equals("FINISHED")) {
      return event.setJobStatus(JobStatus.FINISHED).build();
    }
    logger.info("Invalid Job status " + jobStatus + " for job id: " + jobId);
    throw new JobNotificationEventHandlerException(
        "Invalid Job status: "
            + jobStatus
            + " for sending job completion notification. Job id: "
            + jobId);
  }
}
