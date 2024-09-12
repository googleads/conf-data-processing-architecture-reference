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

variable "project_id" {
  type        = string
  description = "GCP Project ID in which this module will be created."
}

variable "environment" {
  type        = string
  description = "Description for the environment, e.g. dev, staging, production"
}

variable "name" {
  type        = string
  description = "The full ID of the Pub/Sub notifications topic."
}

variable "enable_job_completion_notifications_per_job" {
  type        = bool
  description = "Determines if use Pub/Sub triggered cloud function for job completion notifications."
  default     = false
}

variable "pubsub_triggered_service_account_email" {
  type        = string
  description = "The service account email of pubsub triggered cloud function."
}
