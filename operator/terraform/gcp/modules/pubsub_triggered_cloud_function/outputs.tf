/**
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

output "pubsub_triggered_cloudfunction_url" {
  value       = google_cloudfunctions2_function.pubsub_triggered_cloud_function.service_config[0].uri
  description = "The pubsub triggered cloud function url."
}

output "pubsub_triggered_service_account_email" {
  value       = google_cloudfunctions2_function.pubsub_triggered_cloud_function.service_config[0].service_account_email
  description = "The service account email of pubsub triggered cloud function."
}
