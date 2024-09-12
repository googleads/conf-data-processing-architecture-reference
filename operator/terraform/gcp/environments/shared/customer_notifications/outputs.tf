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

output "notifications_customer_topic_id_1" {
  value       = length(module.notifications_customer_topic_1) > 0 ? module.notifications_customer_topic_1[0].notifications_pubsub_topic_id : ""
  description = "The full ID of the first Pub/Sub customer notifications topic."
}

output "notifications_customer_topic_id_2" {
  value       = length(module.notifications_customer_topic_2) > 0 ? module.notifications_customer_topic_2[0].notifications_pubsub_topic_id : ""
  description = "The full ID of the second Pub/Sub customer notifications topic."
}
