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

terraform {
  required_providers {
    google = {
      source  = "hashicorp/google"
      version = ">= 4.48"
    }
  }
}

provider "google" {
  project = var.project_id
}

module "notifications_customer_topic_1" {
  count  = var.enable_job_completion_notifications_per_job ? 1 : 0
  source = "../../../modules/notifications"
  name   = "${var.environment}-notifications-customer-topic-1"

  environment = var.environment
}

module "notifications_customer_topic_2" {
  count  = var.enable_job_completion_notifications_per_job ? 1 : 0
  source = "../../../modules/notifications"
  name   = "${var.environment}-notifications-customer-topic-2"

  environment = var.environment
}

module "parameter_customer_topic_id_1" {
  count           = var.enable_job_completion_notifications_per_job ? 1 : 0
  source          = "../../../modules/parameters"
  environment     = var.environment
  parameter_name  = "CUSTOMER_TOPIC_ID_1"
  parameter_value = module.notifications_customer_topic_1[0].notifications_pubsub_topic_id
}

module "parameter_customer_topic_id_2" {
  count           = var.enable_job_completion_notifications_per_job ? 1 : 0
  source          = "../../../modules/parameters"
  environment     = var.environment
  parameter_name  = "CUSTOMER_TOPIC_ID_2"
  parameter_value = module.notifications_customer_topic_2[0].notifications_pubsub_topic_id
}

# PubSub read/write permissions
resource "google_pubsub_topic_iam_member" "runtime_cloud_function_pubsub_iam_customer_topic_1" {
  count  = var.enable_job_completion_notifications_per_job ? 1 : 0
  role   = "roles/pubsub.editor"
  member = "serviceAccount:${var.pubsub_triggered_service_account_email}"
  topic  = module.notifications_customer_topic_1[0].notifications_pubsub_topic_id
}

# PubSub read/write permissions
resource "google_pubsub_topic_iam_member" "runtime_cloud_function_pubsub_iam_customer_topic_2" {
  count  = var.enable_job_completion_notifications_per_job ? 1 : 0
  role   = "roles/pubsub.editor"
  member = "serviceAccount:${var.pubsub_triggered_service_account_email}"
  topic  = module.notifications_customer_topic_2[0].notifications_pubsub_topic_id
}
