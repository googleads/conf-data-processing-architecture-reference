# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

locals {
  cloudfunction_name_suffix = "pubsub_triggered_cloud_function"
  cloudfunction_package_zip = "${var.cloud_function_jar}.zip"
}

# Archives the JAR in a ZIP file
data "archive_file" "cloud_function_archive" {
  type        = "zip"
  source_file = var.cloud_function_jar
  output_path = local.cloudfunction_package_zip
}

resource "google_storage_bucket_object" "cloud_function_archive_bucket_object" {
  # Need hash in name so cloudfunction knows to redeploy when code changes
  name   = "${var.environment}_${local.cloudfunction_name_suffix}_${data.archive_file.cloud_function_archive.output_md5}"
  bucket = var.source_bucket_name
  source = local.cloudfunction_package_zip
}

resource "google_cloudfunctions2_function" "pubsub_triggered_cloud_function" {
  name        = var.function_name
  location    = var.region
  description = "[${var.environment}] ${var.description}"
  labels      = { "environment" = var.environment }

  build_config {
    runtime     = "java11"
    entry_point = var.function_entrypoint
    source {
      storage_source {
        bucket = var.source_bucket_name
        object = google_storage_bucket_object.cloud_function_archive_bucket_object.name
      }
    }
  }

  service_config {
    min_instance_count               = var.min_instance_count
    max_instance_count               = var.max_instance_count
    max_instance_request_concurrency = var.concurrency
    available_cpu                    = var.cpu_count
    available_memory                 = "${var.memory_mb}M"
    timeout_seconds                  = 120
    ingress_settings                 = "ALLOW_INTERNAL_AND_GCLB"
    all_traffic_on_latest_revision   = true
    service_account_email            = var.runtime_cloud_function_service_account_email == "" ? google_service_account.runtime_cloud_function_service_account[0].email : var.runtime_cloud_function_service_account_email
    vpc_connector                    = var.vpc_connector_id
    vpc_connector_egress_settings    = var.vpc_connector_id == null ? null : "ALL_TRAFFIC"
  }

  event_trigger {
    trigger_region        = var.region
    event_type            = "google.cloud.pubsub.topic.v1.messagePublished"
    service_account_email = var.event_trigger_service_account_email == "" ? google_service_account.event_trigger_service_account[0].email : var.event_trigger_service_account_email
    pubsub_topic          = var.trigger_pubsub_id
    retry_policy          = var.trigger_pubsub_retry_policy
  }
}

resource "google_service_account" "runtime_cloud_function_service_account" {
  count = var.runtime_cloud_function_service_account_email == "" ? 1 : 0
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-notification"
  display_name = "Service account for the runtime cloud function"
}

resource "google_service_account" "event_trigger_service_account" {
  count = var.event_trigger_service_account_email == "" ? 1 : 0
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-trigger"
  display_name = "Service account for the event trigger in cloud function"
}

resource "google_cloud_run_service_iam_member" "event_trigger_cloud_run_iam_policy" {
  location = google_cloudfunctions2_function.pubsub_triggered_cloud_function.location
  service  = google_cloudfunctions2_function.pubsub_triggered_cloud_function.name

  role = "roles/run.invoker"

  member = "serviceAccount:${var.event_trigger_service_account_email == "" ? google_service_account.event_trigger_service_account[0].email : var.event_trigger_service_account_email}"
}
