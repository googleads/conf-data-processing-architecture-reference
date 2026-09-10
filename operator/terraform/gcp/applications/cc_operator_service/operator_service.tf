/**
 * Copyright 2023-2025 Google LLC
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

locals {
  fe_cloud_run_5xx_errors_alarm_config = ((var.frontend_cloud_run_error_5xx_alarm_config == null) ?
    {
      enable_alarm    = var.alarms_enabled
      eval_period_sec = tonumber(var.frontend_alarm_eval_period_sec)
      duration_sec    = tonumber(var.frontend_alarm_duration_sec)
      error_threshold = tonumber(var.frontend_cloudfunction_5xx_threshold)
    }
  : var.frontend_cloud_run_error_5xx_alarm_config)

  fe_cloud_run_non_5xx_errors_alarm_config = ((var.frontend_cloud_run_non_5xx_error_alarm_config == null) ?
    {
      enable_alarm    = var.alarms_enabled
      eval_period_sec = tonumber(var.frontend_alarm_eval_period_sec)
      duration_sec    = tonumber(var.frontend_alarm_duration_sec)
      error_threshold = tonumber(var.frontend_cloudfunction_error_threshold)
    }
  : var.frontend_cloud_run_non_5xx_error_alarm_config)

  fe_cloud_run_execution_time_alarm_config = ((var.frontend_cloud_run_execution_time_alarm_config == null) ?
    {
      enable_alarm    = var.alarms_enabled
      eval_period_sec = tonumber(var.frontend_alarm_eval_period_sec)
      duration_sec    = tonumber(var.frontend_alarm_duration_sec)
      threshold_ms    = tonumber(var.frontend_cloudfunction_max_execution_time_max)
    }
  : var.frontend_cloud_run_execution_time_alarm_config)
}

# Check to make sure collector regions are the same with server regions.
# This is to encourage users to create collector in the same region as server
# to minimize cross-region traffic. The Cloud NAT will be created for each
# collector region union the worker region. We use "null_resource" here because
# "check" is not available in TF 1.2.3
// TODO: celisun - make otel regions the same as server regions and update the precondition.
resource "null_resource" "check_collector_regions_subset_of_server_regions" {
  lifecycle {
    precondition {
      condition     = !var.enable_opentelemetry_collector || contains(keys(var.collector_regional_config), var.region)
      error_message = "Collector regions must include the server region."
    }
  }
}

module "bazel" {
  source = "../../modules/bazel"
}

module "vpc" {
  source = "../../modules/vpc"

  environment  = var.environment
  project_id   = var.project_id
  regions      = toset([var.region])
  otel_regions = var.enable_opentelemetry_collector ? keys(var.collector_regional_config) : []

  auto_create_subnetworks = var.auto_create_subnetworks
  # These variables are only be used if auto_create_subnetworks is set to false.
  network_name_suffix = var.network_name_suffix
  worker_subnet_cidr  = var.worker_subnet_cidr

  enable_opentelemetry_collector = var.enable_opentelemetry_collector
  collector_subnet_cidr          = var.collector_subnet_cidr
  proxy_subnet_cidr              = var.proxy_subnet_cidr

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
    "CREATE INDEX AsgInstanceStatusIdx ON AsgInstances(Status)",
    "ALTER TABLE AsgInstances ADD COLUMN InstanceGroupName STRING(256)"
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

module "worker_service_account" {
  source = "../../modules/worker/worker_service_account"

  environment                   = var.environment
  project_id                    = var.project_id
  metadatadb_instance_name      = module.jobdatabase.instance_name
  metadatadb_name               = module.jobdatabase.database_name
  user_provided_worker_sa_email = var.user_provided_worker_sa_email
}

module "worker" {
  source                        = "../../modules/worker/cc_worker"
  environment                   = var.environment
  project_id                    = var.project_id
  network                       = module.vpc.network
  subnet_id                     = module.vpc.worker_subnet_ids[var.region]
  egress_internet_tag           = module.vpc.egress_internet_tag
  worker_instance_force_replace = var.worker_instance_force_replace

  metadatadb_instance_name = module.jobdatabase.instance_name
  metadatadb_name          = module.jobdatabase.database_name
  job_queue_sub            = module.jobqueue.queue_pubsub_sub_name
  job_queue_topic          = module.jobqueue.queue_pubsub_topic_name

  instance_type                = var.instance_type
  instance_disk_image_family   = var.instance_disk_image_family
  instance_disk_image          = var.instance_disk_image
  worker_instance_disk_type    = var.worker_instance_disk_type
  worker_instance_disk_size_gb = var.worker_instance_disk_size_gb

  worker_service_account_email = module.worker_service_account.worker_service_account_email

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

  # Make sure the otel collector is running before creating the server instances.
  depends_on = [module.otel_collector]
}

module "autoscaling" {
  source                  = "../../modules/autoscaling"
  environment             = var.environment
  workgroup               = null
  project_id              = var.project_id
  region                  = var.region
  subnet_id               = module.vpc.worker_subnet_ids[var.region]
  vpc_connector_id        = var.vpcsc_compatible ? module.vpc.connectors[var.region] : null
  auto_create_subnetworks = var.auto_create_subnetworks

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
  operator_package_bucket_name        = var.worker_scale_in_path.bucket_name != "" ? var.worker_scale_in_path.bucket_name : google_storage_bucket.operator_package_bucket[0].id
  worker_scale_in_jar                 = local.worker_scale_in_jar
  worker_scale_in_zip                 = var.worker_scale_in_path.zip_file_name
  metadatadb_instance_name            = module.jobdatabase.instance_name
  metadatadb_name                     = module.jobdatabase.database_name
  asg_instances_table_ttl_days        = var.asg_instances_table_ttl_days
  # For Scale-in service alarm
  cloudfunction_5xx_threshold         = var.autoscaling_cloudfunction_5xx_threshold
  cloudfunction_error_threshold       = var.autoscaling_cloudfunction_error_threshold
  cloudfunction_max_execution_time_ms = var.autoscaling_cloudfunction_max_execution_time_ms
  cloudfunction_alarm_eval_period_sec = var.autoscaling_cloudfunction_alarm_eval_period_sec
  cloudfunction_alarm_duration_sec    = var.autoscaling_cloudfunction_alarm_duration_sec

  use_java21_runtime = var.autoscaling_cloudfunction_use_java21_runtime
}

# Storage bucket containing cloudfunction JARs
resource "google_storage_bucket" "operator_package_bucket" {
  count = var.frontend_service_path.bucket_name == "" || var.worker_scale_in_path.bucket_name == "" ? 1 : 0
  # GCS names are globally unique
  name     = "${var.project_id}_${var.environment}_operator_jars"
  location = var.operator_package_bucket_location

  uniform_bucket_level_access = true
}

module "otel_collector" {
  count = var.enable_opentelemetry_collector ? 1 : 0

  source      = "../otel_collector"
  environment = var.environment
  project_id  = var.project_id
  network     = module.vpc.network
  region_zone = var.region_zone

  internet_tag_for_otel = module.vpc.egress_internet_tag

  subnets_per_region            = module.vpc.collector_subnet_ids
  proxy_only_subnets_per_region = module.vpc.proxy_subnet_ids

  collector_regional_config = var.collector_regional_config

  user_provided_collector_sa_email = var.user_provided_collector_sa_email
  collector_instance_type          = var.collector_instance_type
  cos_image_family                 = var.cos_image_family
  otel_collector_startup_config    = var.otel_collector_startup_config
  collector_service_port_name      = var.collector_service_port_name
  collector_service_port           = var.collector_service_port
  collector_min_instance_ready_sec = var.collector_min_instance_ready_sec

  collector_exceed_cpu_usage_alarm         = var.collector_exceed_cpu_usage_alarm
  collector_exceed_memory_usage_alarm      = var.collector_exceed_memory_usage_alarm
  collector_startup_error_alarm            = var.collector_startup_error_alarm
  collector_crash_error_alarm              = var.collector_crash_error_alarm
  export_metric_to_collector_error_alarm   = var.export_metric_to_collector_error_alarm
  collector_queue_size_alarm               = var.collector_queue_size_alarm
  collector_send_metric_failure_rate_alarm = var.collector_send_metric_failure_rate_alarm
  collector_refuse_metric_rate_alarm       = var.collector_refuse_metric_rate_alarm

  collector_service_name = "collector"
  collector_dns_name     = var.collector_dns_name
  collector_domain_name  = var.collector_domain_name
}

module "frontend" {
  source            = "../../modules/frontend"
  environment       = var.environment
  project_id        = var.project_id
  region            = var.region
  vpc_connector_ids = var.vpcsc_compatible ? module.vpc.connectors : {}

  job_version                 = var.job_version
  spanner_instance_name       = module.jobdatabase.instance_name
  spanner_database_name       = module.jobdatabase.database_name
  job_table_name              = var.job_client_parameter_values.job_table_name
  job_metadata_table_ttl_days = var.job_table_ttl_days
  job_queue_topic             = module.jobqueue.queue_pubsub_topic_id
  job_queue_sub               = module.jobqueue.queue_pubsub_sub_id

  create_frontend_service_cloud_function = var.create_frontend_service_cloud_function
  operator_package_bucket_name           = var.frontend_service_path.bucket_name != "" ? var.frontend_service_path.bucket_name : google_storage_bucket.operator_package_bucket[0].id
  frontend_service_jar                   = local.frontend_service_jar
  frontend_service_zip                   = var.frontend_service_path.zip_file_name

  frontend_service_cloudfunction_num_cpus                         = var.frontend_service_cloudfunction_num_cpus
  frontend_service_cloudfunction_memory_mb                        = var.frontend_service_cloudfunction_memory_mb
  frontend_service_cloudfunction_min_instances                    = var.frontend_service_cloudfunction_min_instances
  frontend_service_cloudfunction_max_instances                    = var.frontend_service_cloudfunction_max_instances
  frontend_service_cloudfunction_max_instance_request_concurrency = var.frontend_service_cloudfunction_max_instance_request_concurrency
  frontend_service_cloudfunction_timeout_sec                      = var.frontend_service_cloudfunction_timeout_sec
  frontend_service_cloudfunction_runtime_sa_email                 = var.frontend_service_cloudfunction_runtime_sa_email

  alarms_enabled                       = var.alarms_enabled
  notification_channel_id              = local.notification_channel_id
  alarm_duration_sec                   = var.frontend_alarm_duration_sec
  alarm_eval_period_sec                = var.frontend_alarm_eval_period_sec
  cloudfunction_5xx_threshold          = var.frontend_cloudfunction_5xx_threshold
  cloudfunction_error_threshold        = var.frontend_cloudfunction_error_threshold
  cloudfunction_max_execution_time_max = var.frontend_cloudfunction_max_execution_time_max
  lb_5xx_threshold                     = var.frontend_lb_5xx_threshold
  lb_max_latency_ms                    = var.frontend_lb_max_latency_ms

  cloud_run_error_5xx_alarm_config      = local.fe_cloud_run_5xx_errors_alarm_config
  cloud_run_non_5xx_error_alarm_config  = local.fe_cloud_run_non_5xx_errors_alarm_config
  cloud_run_execution_time_alarm_config = local.fe_cloud_run_execution_time_alarm_config

  use_java21_runtime = var.frontend_service_cloudfunction_use_java21_runtime

  frontend_service_cloud_run_regions                     = var.frontend_service_cloud_run_regions
  frontend_service_cloud_run_deletion_protection         = var.frontend_service_cloud_run_deletion_protection
  frontend_service_cloud_run_source_container_image_url  = var.frontend_service_cloud_run_source_container_image_url
  frontend_service_cloud_run_cpu_idle                    = var.frontend_service_cloud_run_cpu_idle
  frontend_service_cloud_run_startup_cpu_boost           = var.frontend_service_cloud_run_startup_cpu_boost
  frontend_service_cloud_run_ingress_traffic_setting     = var.frontend_service_cloud_run_ingress_traffic_setting
  frontend_service_cloud_run_allowed_invoker_iam_members = var.frontend_service_cloud_run_allowed_invoker_iam_members
  frontend_service_cloud_run_binary_authorization        = var.frontend_service_cloud_run_binary_authorization
  frontend_service_cloud_run_custom_audiences            = var.frontend_service_cloud_run_custom_audiences

  frontend_service_enable_lb_backend_logging = var.frontend_service_enable_lb_backend_logging
  frontend_service_lb_allowed_request_paths  = var.frontend_service_lb_allowed_request_paths
  frontend_service_lb_domain                 = var.frontend_service_lb_domain

  frontend_service_lb_outlier_detection_interval_seconds                      = var.frontend_service_lb_outlier_detection_interval_seconds
  frontend_service_lb_outlier_detection_base_ejection_time_seconds            = var.frontend_service_lb_outlier_detection_base_ejection_time_seconds
  frontend_service_lb_outlier_detection_consecutive_errors                    = var.frontend_service_lb_outlier_detection_consecutive_errors
  frontend_service_lb_outlier_detection_enforcing_consecutive_errors          = var.frontend_service_lb_outlier_detection_enforcing_consecutive_errors
  frontend_service_lb_outlier_detection_consecutive_gateway_failure           = var.frontend_service_lb_outlier_detection_consecutive_gateway_failure
  frontend_service_lb_outlier_detection_enforcing_consecutive_gateway_failure = var.frontend_service_lb_outlier_detection_enforcing_consecutive_gateway_failure
  frontend_service_lb_outlier_detection_max_ejection_percent                  = var.frontend_service_lb_outlier_detection_max_ejection_percent

  frontend_service_parent_domain_name            = var.frontend_service_parent_domain_name
  frontend_service_parent_domain_name_project_id = var.frontend_service_parent_domain_name_project_id

  lb_error_5xx_alarm_config         = var.frontend_lb_error_5xx_alarm_config
  lb_non_5xx_error_alarm_config     = var.frontend_lb_non_5xx_error_alarm_config
  lb_request_latencies_alarm_config = var.frontend_lb_request_latencies_alarm_config
}

moved {
  from = module.opentelemetry_collector
  to   = module.otel_collector[0].module.otel_collector
}

moved {
  from = module.opentelemetry_collector_load_balancer
  to   = module.otel_collector[0].module.otel_collector_load_balancer
}
