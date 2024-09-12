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

module "base_worker" {
  source                        = "../base_worker"
  environment                   = var.environment
  project_id                    = var.project_id
  network                       = var.network
  egress_internet_tag           = var.egress_internet_tag
  worker_instance_force_replace = var.worker_instance_force_replace

  metadatadb_instance_name = var.metadatadb_instance_name
  metadatadb_name          = var.metadatadb_name
  job_queue_sub            = var.job_queue_sub
  job_queue_topic          = var.job_queue_topic

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

  autoscaler_cloudfunction_name = var.autoscaler_cloudfunction_name
  autoscaler_name               = var.autoscaler_name
  vm_instance_group_name        = var.vm_instance_group_name

  alarms_enabled          = var.alarms_enabled
  alarm_duration_sec      = var.alarm_duration_sec
  alarm_eval_period_sec   = var.alarm_eval_period_sec
  notification_channel_id = var.notification_channel_id
}

module "java_custom_monitoring" {
  source                  = "../../java_custom_monitoring"
  count                   = var.java_custom_metrics_alarms_enabled ? 1 : 0
  environment             = var.environment
  vm_instance_group_name  = var.vm_instance_group_name
  alarm_duration_sec      = var.alarm_duration_sec
  alarm_eval_period_sec   = var.alarm_eval_period_sec
  notification_channel_id = var.notification_channel_id
}
