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

module "job_service" {
  source      = "../../applications/jobservice"
  environment = var.environment
  project_id  = var.project_id
  region      = var.region
  region_zone = var.region_zone

  # Global Alarms
  alarms_enabled            = var.alarms_enabled
  alarms_notification_email = var.alarms_notification_email

  # Spanner DB
  spanner_instance_config              = var.spanner_instance_config
  spanner_processing_units             = var.spanner_processing_units
  spanner_database_deletion_protection = var.spanner_database_deletion_protection

  # Frontend Service
  operator_package_bucket_location                                = var.operator_package_bucket_location
  frontend_service_jar                                            = var.frontend_service_jar
  frontend_service_cloudfunction_num_cpus                         = var.frontend_service_cloudfunction_num_cpus
  frontend_service_cloudfunction_memory_mb                        = var.frontend_service_cloudfunction_memory_mb
  frontend_service_cloudfunction_min_instances                    = var.frontend_service_cloudfunction_min_instances
  frontend_service_cloudfunction_max_instances                    = var.frontend_service_cloudfunction_max_instances
  frontend_service_cloudfunction_max_instance_request_concurrency = var.frontend_service_cloudfunction_max_instance_request_concurrency
  frontend_service_cloudfunction_timeout_sec                      = var.frontend_service_cloudfunction_timeout_sec
  job_version                                                     = var.job_version

  # Frontend Service Alarms
  frontend_alarm_duration_sec                   = var.frontend_alarm_duration_sec
  frontend_alarm_eval_period_sec                = var.frontend_alarm_eval_period_sec
  frontend_cloudfunction_5xx_threshold          = var.frontend_cloudfunction_5xx_threshold
  frontend_cloudfunction_error_threshold        = var.frontend_cloudfunction_error_threshold
  frontend_cloudfunction_max_execution_time_max = var.frontend_cloudfunction_max_execution_time_max
  frontend_lb_5xx_threshold                     = var.frontend_lb_5xx_threshold
  frontend_lb_max_latency_ms                    = var.frontend_lb_max_latency_ms
  job_metadata_table_ttl_days                   = var.job_metadata_table_ttl_days

  # Worker
  instance_type                 = var.instance_type
  instance_disk_image           = var.instance_disk_image
  worker_instance_disk_type     = var.worker_instance_disk_type
  worker_instance_disk_size_gb  = var.worker_instance_disk_size_gb
  max_job_processing_time       = var.max_job_processing_time
  max_job_num_attempts          = var.max_job_num_attempts
  user_provided_worker_sa_email = var.user_provided_worker_sa_email
  worker_instance_force_replace = var.worker_instance_force_replace

  # Worker Alarms
  worker_alarm_duration_sec    = var.worker_alarm_duration_sec
  worker_alarm_eval_period_sec = var.worker_alarm_eval_period_sec

  # Instance Metadata
  worker_logging_enabled           = var.worker_logging_enabled
  worker_monitoring_enabled        = var.worker_monitoring_enabled
  worker_container_log_redirect    = var.worker_container_log_redirect
  worker_memory_monitoring_enabled = var.worker_memory_monitoring_enabled
  worker_image                     = var.worker_image
  worker_image_signature_repos     = var.worker_image_signature_repos
  worker_restart_policy            = var.worker_restart_policy
  allowed_operator_service_account = var.allowed_operator_service_account

  # Autoscaling
  min_worker_instances                = var.min_worker_instances
  max_worker_instances                = var.max_worker_instances
  autoscaling_jobs_per_instance       = var.autoscaling_jobs_per_instance
  autoscaling_cloudfunction_memory_mb = var.autoscaling_cloudfunction_memory_mb
  worker_scale_in_jar                 = var.worker_scale_in_jar
  termination_wait_timeout_sec        = var.termination_wait_timeout_sec
  worker_scale_in_cron                = var.worker_scale_in_cron
  asg_instances_table_ttl_days        = var.asg_instances_table_ttl_days

  # Autoscaling Alarms
  autoscaling_alarm_duration_sec                  = var.autoscaling_alarm_duration_sec
  autoscaling_alarm_eval_period_sec               = var.autoscaling_alarm_eval_period_sec
  autoscaling_cloudfunction_5xx_threshold         = var.autoscaling_cloudfunction_5xx_threshold
  autoscaling_cloudfunction_error_threshold       = var.autoscaling_cloudfunction_error_threshold
  autoscaling_cloudfunction_max_execution_time_ms = var.autoscaling_cloudfunction_max_execution_time_ms
  autoscaling_max_vm_instances_ratio_threshold    = var.autoscaling_max_vm_instances_ratio_threshold

  # Job Queue Alarms
  jobqueue_alarm_eval_period_sec           = var.jobqueue_alarm_eval_period_sec
  jobqueue_max_undelivered_message_age_sec = var.jobqueue_max_undelivered_message_age_sec

  # VPC Service Control
  vpcsc_compatible           = var.vpcsc_compatible
  vpc_connector_machine_type = var.vpc_connector_machine_type

  # Notifications
  enable_job_completion_notifications = var.enable_job_completion_notifications

  enable_job_completion_notifications_per_job           = var.enable_job_completion_notifications_per_job
  job_completion_notifications_cloud_function_jar       = var.job_completion_notifications_cloud_function_jar
  job_completion_notifications_cloud_function_cpu_count = var.job_completion_notifications_cloud_function_cpu_count
  job_completion_notifications_cloud_function_memory_mb = var.job_completion_notifications_cloud_function_memory_mb
  job_completion_notifications_service_account_email    = var.job_completion_notifications_service_account_email
}