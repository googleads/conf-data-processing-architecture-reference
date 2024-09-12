/**
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
  region  = var.region
  zone    = var.region_zone
}

module "bazel" {
  source = "../../modules/bazel"
}

module "vpc" {
  source = "../../modules/vpc"

  environment = var.environment
  project_id  = var.project_id
  regions     = toset([var.region])

  create_connectors      = var.vpcsc_compatible
  connector_machine_type = var.vpc_connector_machine_type
}

locals {
  frontend_service_jar         = var.frontend_service_jar != "" ? var.frontend_service_jar : "${module.bazel.bazel_bin}/java/com/google/scp/operator/frontend/service/gcp/FrontendServiceHttpCloudFunction_deploy.jar"
  worker_scale_in_jar          = var.worker_scale_in_jar != "" ? var.worker_scale_in_jar : "${module.bazel.bazel_bin}/java/com/google/scp/operator/autoscaling/app/gcp/WorkerScaleInCloudFunction_deploy.jar"
  job_notification_service_jar = var.job_completion_notifications_cloud_function_jar != "" ? var.job_completion_notifications_cloud_function_jar : "${module.bazel.bazel_bin}/java/com/google/scp/operator/notification/service/gcp/JobNotificationCloudFunction_deploy.jar"
  notification_channel_id      = var.alarms_enabled ? google_monitoring_notification_channel.alarm_email[0].id : null
}

# Storage bucket containing cloudfunction JARs
resource "google_storage_bucket" "operator_package_bucket" {
  # GCS names are globally unique
  name     = "${var.project_id}_${var.environment}_operator_jars"
  location = var.operator_package_bucket_location

  uniform_bucket_level_access = true
}

resource "google_monitoring_notification_channel" "alarm_email" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Operator Alarms Notification Email"
  type         = "email"
  labels = {
    email_address = var.alarms_notification_email
  }
  force_delete = true

  lifecycle {
    # Email should not be empty
    precondition {
      condition     = var.alarms_notification_email != ""
      error_message = "var.alarms_enabled is true with an empty var.alarms_notification_email."
    }
  }
}

module "metadatadb" {
  source      = "../../modules/metadatadb"
  environment = var.environment

  spanner_instance_config              = var.spanner_instance_config
  spanner_processing_units             = var.spanner_processing_units
  spanner_database_deletion_protection = var.spanner_database_deletion_protection
}

module "jobqueue" {
  source      = "../../modules/jobqueue"
  environment = var.environment

  alarms_enabled                  = var.alarms_enabled
  notification_channel_id         = local.notification_channel_id
  alarm_eval_period_sec           = var.jobqueue_alarm_eval_period_sec
  max_undelivered_message_age_sec = var.jobqueue_max_undelivered_message_age_sec
}

module "metadata_db_instance_id" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "JOB_SPANNER_INSTANCE_ID"
  parameter_value = module.metadatadb.metadatadb_instance_name
}

module "metadata_db_name" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "JOB_SPANNER_DB_NAME"
  parameter_value = module.metadatadb.metadatadb_name
}

module "job_queue_topic_id" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "JOB_PUBSUB_TOPIC_ID"
  parameter_value = module.jobqueue.jobqueue_pubsub_topic_name
}

module "job_queue_subscription_id" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "JOB_PUBSUB_SUBSCRIPTION_ID"
  parameter_value = module.jobqueue.jobqueue_pubsub_sub_name
}

module "max_job_processing_time" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "MAX_JOB_PROCESSING_TIME_SECONDS"
  parameter_value = var.max_job_processing_time
}

module "max_job_num_attempts" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "MAX_JOB_NUM_ATTEMPTS"
  parameter_value = var.max_job_num_attempts
}

module "worker_managed_instance_group_name" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "WORKER_MANAGED_INSTANCE_GROUP_NAME"
  parameter_value = module.autoscaling.worker_managed_instance_group_name
}

module "notifications_topic_id" {
  count           = var.enable_job_completion_notifications ? 1 : 0
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "NOTIFICATIONS_TOPIC_ID"
  parameter_value = module.notifications[0].notifications_pubsub_topic_id
}

module "frontend" {
  source           = "../../modules/frontend"
  environment      = var.environment
  project_id       = var.project_id
  region           = var.region
  vpc_connector_id = var.vpcsc_compatible ? module.vpc.connectors[var.region] : null
  job_version      = var.job_version

  spanner_instance_name       = module.metadatadb.metadatadb_instance_name
  spanner_database_name       = module.metadatadb.metadatadb_name
  job_table_name              = "JobMetadata"
  job_metadata_table_ttl_days = var.job_metadata_table_ttl_days
  job_queue_topic             = module.jobqueue.jobqueue_pubsub_topic_name
  job_queue_sub               = module.jobqueue.jobqueue_pubsub_sub_name

  operator_package_bucket_name = google_storage_bucket.operator_package_bucket.id
  frontend_service_jar         = local.frontend_service_jar

  frontend_service_cloudfunction_num_cpus                         = var.frontend_service_cloudfunction_num_cpus
  frontend_service_cloudfunction_memory_mb                        = var.frontend_service_cloudfunction_memory_mb
  frontend_service_cloudfunction_min_instances                    = var.frontend_service_cloudfunction_min_instances
  frontend_service_cloudfunction_max_instances                    = var.frontend_service_cloudfunction_max_instances
  frontend_service_cloudfunction_max_instance_request_concurrency = var.frontend_service_cloudfunction_max_instance_request_concurrency
  frontend_service_cloudfunction_timeout_sec                      = var.frontend_service_cloudfunction_timeout_sec

  alarms_enabled                       = var.alarms_enabled
  notification_channel_id              = local.notification_channel_id
  alarm_duration_sec                   = var.frontend_alarm_duration_sec
  alarm_eval_period_sec                = var.frontend_alarm_eval_period_sec
  cloudfunction_5xx_threshold          = var.frontend_cloudfunction_5xx_threshold
  cloudfunction_error_threshold        = var.frontend_cloudfunction_error_threshold
  cloudfunction_max_execution_time_max = var.frontend_cloudfunction_max_execution_time_max
  lb_5xx_threshold                     = var.frontend_lb_5xx_threshold
  lb_max_latency_ms                    = var.frontend_lb_max_latency_ms
}

module "worker" {
  source                        = "../../modules/worker/java_worker"
  environment                   = var.environment
  project_id                    = var.project_id
  network                       = module.vpc.network
  egress_internet_tag           = module.vpc.egress_internet_tag
  worker_instance_force_replace = var.worker_instance_force_replace

  metadatadb_instance_name = module.metadatadb.metadatadb_instance_name
  metadatadb_name          = module.metadatadb.metadatadb_name
  job_queue_sub            = module.jobqueue.jobqueue_pubsub_sub_name
  job_queue_topic          = module.jobqueue.jobqueue_pubsub_topic_name

  instance_type                = var.instance_type
  instance_disk_image          = var.instance_disk_image
  worker_instance_disk_type    = var.worker_instance_disk_type
  worker_instance_disk_size_gb = var.worker_instance_disk_size_gb

  user_provided_worker_sa_email = var.user_provided_worker_sa_email

  # Instance Metadata
  worker_logging_enabled           = var.worker_logging_enabled
  worker_monitoring_enabled        = var.worker_monitoring_enabled
  worker_container_log_redirect    = var.worker_container_log_redirect
  worker_memory_monitoring_enabled = var.worker_memory_monitoring_enabled
  worker_image                     = var.worker_image
  worker_image_signature_repos     = var.worker_image_signature_repos
  worker_restart_policy            = var.worker_restart_policy
  allowed_operator_service_account = var.allowed_operator_service_account

  autoscaler_cloudfunction_name = module.autoscaling.autoscaler_cloudfunction_name
  autoscaler_name               = module.autoscaling.autoscaler_name
  vm_instance_group_name        = module.autoscaling.worker_managed_instance_group_name

  alarms_enabled                     = var.alarms_enabled
  java_custom_metrics_alarms_enabled = var.alarms_enabled
  alarm_duration_sec                 = var.worker_alarm_duration_sec
  alarm_eval_period_sec              = var.worker_alarm_eval_period_sec
  notification_channel_id            = local.notification_channel_id
}

module "autoscaling" {
  source           = "../../modules/autoscaling"
  environment      = var.environment
  project_id       = var.project_id
  region           = var.region
  vpc_connector_id = var.vpcsc_compatible ? module.vpc.connectors[var.region] : null

  worker_template                     = module.worker.worker_template
  min_worker_instances                = var.min_worker_instances
  max_worker_instances                = var.max_worker_instances
  jobqueue_subscription_name          = module.jobqueue.jobqueue_pubsub_sub_name
  autoscaling_jobs_per_instance       = var.autoscaling_jobs_per_instance
  autoscaling_cloudfunction_memory_mb = var.autoscaling_cloudfunction_memory_mb
  worker_service_account              = module.worker.worker_service_account_email
  termination_wait_timeout_sec        = var.termination_wait_timeout_sec
  worker_scale_in_cron                = var.worker_scale_in_cron

  operator_package_bucket_name = google_storage_bucket.operator_package_bucket.id
  worker_scale_in_jar          = local.worker_scale_in_jar

  metadatadb_instance_name     = module.metadatadb.metadatadb_instance_name
  metadatadb_name              = module.metadatadb.metadatadb_name
  asg_instances_table_ttl_days = var.asg_instances_table_ttl_days

  alarms_enabled                      = var.alarms_enabled
  notification_channel_id             = local.notification_channel_id
  alarm_eval_period_sec               = var.autoscaling_alarm_eval_period_sec
  alarm_duration_sec                  = var.autoscaling_alarm_duration_sec
  max_vm_instances_ratio_threshold    = var.autoscaling_max_vm_instances_ratio_threshold
  cloudfunction_5xx_threshold         = var.autoscaling_cloudfunction_5xx_threshold
  cloudfunction_error_threshold       = var.autoscaling_cloudfunction_error_threshold
  cloudfunction_max_execution_time_ms = var.autoscaling_cloudfunction_max_execution_time_ms
}

module "notifications" {
  count  = var.enable_job_completion_notifications ? 1 : 0
  source = "../../modules/notifications"
  name   = "${var.environment}-notifications"

  environment = var.environment
}

module "job_completion_notifications" {
  count  = var.enable_job_completion_notifications || var.enable_job_completion_notifications_per_job ? 1 : 0
  source = "../../modules/notifications"
  name   = "${var.environment}-job-completion-notifications"

  environment = var.environment
}

module "job_completion_notifications_internal_topic_id" {
  count           = var.enable_job_completion_notifications || var.enable_job_completion_notifications_per_job ? 1 : 0
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "JOB_COMPLETION_NOTIFICATIONS_TOPIC_ID"
  parameter_value = module.job_completion_notifications[0].notifications_pubsub_topic_id
}

module "job_completion_notifications_cloud_function" {
  count               = var.enable_job_completion_notifications || var.enable_job_completion_notifications_per_job ? 1 : 0
  source              = "../../modules/pubsub_triggered_cloud_function"
  environment         = var.environment
  function_name       = "${var.environment}-job-notification-handler"
  region              = var.region
  description         = "Handler to send notification to custom PubSub topics."
  function_entrypoint = "com.google.scp.operator.notification.service.gcp.JobNotificationEventHandler"
  source_bucket_name  = google_storage_bucket.operator_package_bucket.id
  cloud_function_jar  = var.job_completion_notifications_cloud_function_jar
  vpc_connector_id    = var.vpcsc_compatible ? module.vpc.connectors[var.region] : null

  min_instance_count          = 0
  max_instance_count          = 1
  concurrency                 = 1
  cpu_count                   = var.job_completion_notifications_cloud_function_cpu_count
  memory_mb                   = var.job_completion_notifications_cloud_function_memory_mb
  trigger_pubsub_id           = module.job_completion_notifications[0].notifications_pubsub_topic_id
  trigger_pubsub_retry_policy = "RETRY_POLICY_DO_NOT_RETRY" # It is fine to wait until the next invocation

  runtime_cloud_function_service_account_email = var.job_completion_notifications_service_account_email
  event_trigger_service_account_email          = var.job_completion_notifications_service_account_email
}

# PubSub read/write permissions for global notification topic
resource "google_pubsub_topic_iam_member" "job_completion_notifications_cloud_function_iam_global_notification" {
  count  = var.enable_job_completion_notifications ? 1 : 0
  role   = "roles/pubsub.publisher"
  member = "serviceAccount:${module.job_completion_notifications_cloud_function[0].pubsub_triggered_service_account_email}"
  topic  = module.notifications[0].notifications_pubsub_topic_id
}
