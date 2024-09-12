/**
 * Copyright 2023 Google LLC
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
      version = ">= 4.36"
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
  frontend_service_jar    = var.frontend_service_jar == null ? "${module.bazel.bazel_bin}/java/com/google/scp/operator/frontend/service/gcp/FrontendServiceHttpCloudFunction_deploy.jar" : var.frontend_service_jar
  worker_scale_in_jar     = var.worker_scale_in_jar == null ? "${module.bazel.bazel_bin}/java/com/google/scp/operator/autoscaling/app/gcp/WorkerScaleInCloudFunction_deploy.jar" : var.worker_scale_in_jar
  any_alarms_enabled      = var.alarms_enabled || var.custom_metrics_alarms_enabled
  notification_channel_id = local.any_alarms_enabled ? google_monitoring_notification_channel.alarm_email[0].id : null
}

resource "google_monitoring_notification_channel" "alarm_email" {
  count        = local.any_alarms_enabled ? 1 : 0
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
      error_message = "var.alarms_enabled or var.java_custom_metrics_alarms_enabled or var.cc_custom_metrics_alarms_enabled is true with an empty var.alarms_notification_email."
    }
  }
}

module "jobdatabase" {
  source        = "../../modules/database"
  environment   = var.environment
  instance_name = "${var.environment}-${var.job_client_parameter_values.job_spanner_instance_name}"
  database_name = var.job_client_parameter_values.job_spanner_database_name
  database_schema = [
    <<-EOT
    CREATE TABLE ${var.job_client_parameter_values.job_table_name} (
      JobId STRING(MAX) NOT NULL,
      Value JSON NOT NULL,
      Ttl TIMESTAMP NOT NULL,
    )
    PRIMARY KEY (JobId),
    ROW DELETION POLICY (OLDER_THAN(Ttl, INTERVAL 0 DAY))
    EOT
    ,
    <<-EOT
    CREATE TABLE ${var.auto_scaling_client_parameter_values.instance_table_name} (
      InstanceName STRING(256) NOT NULL,
      Status STRING(64) NOT NULL,
      RequestTime TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp=true),
      TerminationTime TIMESTAMP OPTIONS (allow_commit_timestamp=true),
      Ttl TIMESTAMP NOT NULL,
      TerminationReason STRING(64),
    )
    PRIMARY KEY (InstanceName),
    ROW DELETION POLICY (OLDER_THAN(Ttl, INTERVAL 0 DAY))
    EOT
    ,
    "CREATE INDEX AsgInstanceStatusIdx ON AsgInstances(Status)"
  ]

  spanner_instance_config              = var.spanner_instance_config
  spanner_processing_units             = var.spanner_processing_units
  spanner_database_deletion_protection = var.spanner_database_deletion_protection
}

locals {
  job_queue_name = "${var.environment}-${var.job_client_parameter_values.job_queue_name}"
}

module "jobqueue" {
  source      = "../../modules/queue"
  environment = var.environment
  # the topic name and subscription name are sharing the same value
  # for cc_operator_service.
  topic_name        = local.job_queue_name
  subscription_name = local.job_queue_name

  alarms_enabled                  = var.alarms_enabled
  notification_channel_id         = local.notification_channel_id
  alarm_eval_period_sec           = var.jobqueue_alarm_eval_period_sec
  max_undelivered_message_age_sec = var.jobqueue_max_undelivered_message_age_sec
}

module "worker" {
  source                        = "../../modules/worker/cc_worker"
  environment                   = var.environment
  project_id                    = var.project_id
  network                       = module.vpc.network
  egress_internet_tag           = module.vpc.egress_internet_tag
  worker_instance_force_replace = var.worker_instance_force_replace

  metadatadb_instance_name = module.jobdatabase.instance_name
  metadatadb_name          = module.jobdatabase.database_name
  job_queue_sub            = module.jobqueue.queue_pubsub_sub_name
  job_queue_topic          = module.jobqueue.queue_pubsub_topic_name

  instance_type                = var.instance_type
  instance_disk_image          = var.instance_disk_image
  worker_instance_disk_type    = var.worker_instance_disk_type
  worker_instance_disk_size_gb = var.worker_instance_disk_size_gb

  user_provided_worker_sa_email = var.user_provided_worker_sa_email

  # Instance Metadata
  worker_logging_enabled           = var.worker_logging_enabled
  worker_monitoring_enabled        = var.worker_monitoring_enabled
  worker_image                     = var.worker_image
  worker_image_signature_repos     = var.worker_image_signature_repos
  worker_restart_policy            = var.worker_restart_policy
  allowed_operator_service_account = var.allowed_operator_service_account
  worker_memory_monitoring_enabled = var.worker_memory_monitoring_enabled
  worker_container_log_redirect    = var.worker_container_log_redirect

  autoscaler_name        = module.autoscaling.autoscaler_name
  vm_instance_group_name = module.autoscaling.worker_managed_instance_group_name

  alarms_enabled                   = var.alarms_enabled
  cc_custom_metrics_alarms_enabled = var.custom_metrics_alarms_enabled
  alarm_duration_sec               = var.worker_alarm_duration_sec
  alarm_eval_period_sec            = var.worker_alarm_eval_period_sec
  notification_channel_id          = local.notification_channel_id

  # Worker alarm threshold
  joblifecyclehelper_job_completion_failure_threshold = var.joblifecyclehelper_job_completion_failure_threshold
  joblifecyclehelper_job_extender_failure_threshold   = var.joblifecyclehelper_job_extender_failure_threshold
  joblifecyclehelper_job_pool_failure_threshold       = var.joblifecyclehelper_job_pool_failure_threshold
  joblifecyclehelper_job_processing_time_threshold    = var.joblifecyclehelper_job_processing_time_threshold
  joblifecyclehelper_job_pulling_failure_threshold    = var.joblifecyclehelper_job_pulling_failure_threshold
  joblifecyclehelper_job_waiting_time_threshold       = var.joblifecyclehelper_job_waiting_time_threshold
  joblifecyclehelper_job_release_failure_threshold    = var.joblifecyclehelper_job_release_failure_threshold

  # Variables not valid for cc_operator_service but required in the child module
  autoscaler_cloudfunction_name = ""
}

module "autoscaling" {
  source           = "../../modules/autoscaling"
  environment      = var.environment
  project_id       = var.project_id
  region           = var.region
  vpc_connector_id = var.vpcsc_compatible ? module.vpc.connectors[var.region] : null

  worker_template               = module.worker.worker_template
  min_worker_instances          = var.min_worker_instances
  max_worker_instances          = var.max_worker_instances
  jobqueue_subscription_name    = module.jobqueue.queue_pubsub_sub_name
  autoscaling_jobs_per_instance = var.autoscaling_jobs_per_instance

  alarms_enabled                   = var.alarms_enabled
  notification_channel_id          = local.notification_channel_id
  alarm_eval_period_sec            = var.autoscaling_alarm_eval_period_sec
  alarm_duration_sec               = var.autoscaling_alarm_duration_sec
  max_vm_instances_ratio_threshold = var.autoscaling_max_vm_instances_ratio_threshold

  # For Scale-in service.
  autoscaling_cloudfunction_memory_mb = var.autoscaling_cloudfunction_memory_mb
  worker_service_account              = module.worker.worker_service_account_email
  termination_wait_timeout_sec        = var.termination_wait_timeout_sec
  worker_scale_in_cron                = var.worker_scale_in_cron
  operator_package_bucket_name        = google_storage_bucket.operator_package_bucket.id
  worker_scale_in_jar                 = local.worker_scale_in_jar
  metadatadb_instance_name            = module.jobdatabase.instance_name
  metadatadb_name                     = module.jobdatabase.database_name
  asg_instances_table_ttl_days        = var.asg_instances_table_ttl_days
  # For Scale-in service alarm
  cloudfunction_5xx_threshold         = var.autoscaling_cloudfunction_5xx_threshold
  cloudfunction_error_threshold       = var.autoscaling_cloudfunction_error_threshold
  cloudfunction_max_execution_time_ms = var.autoscaling_cloudfunction_max_execution_time_ms
}

# Storage bucket containing cloudfunction JARs
resource "google_storage_bucket" "operator_package_bucket" {
  # GCS names are globally unique
  name     = "${var.project_id}_${var.environment}_operator_jars"
  location = var.operator_package_bucket_location

  uniform_bucket_level_access = true
}

module "frontend" {
  source           = "../../modules/frontend"
  environment      = var.environment
  project_id       = var.project_id
  region           = var.region
  vpc_connector_id = var.vpcsc_compatible ? module.vpc.connectors[var.region] : null

  job_version                 = var.job_version
  spanner_instance_name       = module.jobdatabase.instance_name
  spanner_database_name       = module.jobdatabase.database_name
  job_table_name              = var.job_client_parameter_values.job_table_name
  job_metadata_table_ttl_days = var.job_table_ttl_days
  job_queue_topic             = module.jobqueue.queue_pubsub_topic_name
  job_queue_sub               = module.jobqueue.queue_pubsub_sub_name

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
