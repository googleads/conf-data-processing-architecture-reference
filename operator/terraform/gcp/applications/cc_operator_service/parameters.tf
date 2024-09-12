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

################################################################################
# Write parameters into cloud.
################################################################################

##### shared parameters start #####
module "common_log_option" {
  count           = var.common_parameter_values.log_option == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.common_parameter_names.log_option
  parameter_value = var.common_parameter_values.log_option
}

module "common_enabled_log_levels" {
  count           = var.common_parameter_values.enabled_log_levels == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.common_parameter_names.enabled_log_levels
  parameter_value = var.common_parameter_values.enabled_log_levels
}

module "common_cpu_thread_count" {
  count           = var.common_parameter_values.cpu_thread_count == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.common_parameter_names.cpu_thread_count
  parameter_value = var.common_parameter_values.cpu_thread_count
}

module "common_cpu_thread_pool_queue_cap" {
  count           = var.common_parameter_values.cpu_thread_pool_queue_cap == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.common_parameter_names.cpu_thread_pool_queue_cap
  parameter_value = var.common_parameter_values.cpu_thread_pool_queue_cap
}

module "common_io_thread_count" {
  count           = var.common_parameter_values.io_thread_count == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.common_parameter_names.io_thread_count
  parameter_value = var.common_parameter_values.io_thread_count
}

module "common_io_thread_pool_queue_cap" {
  count           = var.common_parameter_values.io_thread_pool_queue_cap == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.common_parameter_names.io_thread_pool_queue_cap
  parameter_value = var.common_parameter_values.io_thread_pool_queue_cap
}
##### shared parameters start #####

##### JobLifecycleHelper parameters start #####
module "job_lifecycle_helper_retry_limit" {
  count           = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_retry_limit == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_lifecycle_helper_parameter_names.job_lifecycle_helper_retry_limit
  parameter_value = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_retry_limit
}

module "job_lifecycle_helper_visibility_timeout_extend_time" {
  count           = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_visibility_timeout_extend_time == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_lifecycle_helper_parameter_names.job_lifecycle_helper_visibility_timeout_extend_time
  parameter_value = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_visibility_timeout_extend_time
}

module "job_lifecycle_helper_job_processing_timeout" {
  count           = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_job_processing_timeout == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_lifecycle_helper_parameter_names.job_lifecycle_helper_job_processing_timeout
  parameter_value = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_job_processing_timeout
}

module "job_lifecycle_helper_job_extending_worker_sleep_time" {
  count           = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_job_extending_worker_sleep_time == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_lifecycle_helper_parameter_names.job_lifecycle_helper_job_extending_worker_sleep_time
  parameter_value = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_job_extending_worker_sleep_time
}

module "job_lifecycle_helper_enable_metric_recording" {
  count           = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_enable_metric_recording == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_lifecycle_helper_parameter_names.job_lifecycle_helper_enable_metric_recording
  parameter_value = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_enable_metric_recording
}

module "job_lifecycle_helper_metric_namespace" {
  count           = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_metric_namespace == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_lifecycle_helper_parameter_names.job_lifecycle_helper_metric_namespace
  parameter_value = var.job_lifecycle_helper_parameter_values.job_lifecycle_helper_metric_namespace
}
##### JobLifecycleHelper parameters end #####

##### JobClient parameters start #####
module "job_spanner_instance_name" {
  count          = var.job_client_parameter_values.job_spanner_instance_name == null ? 0 : 1
  source         = "../../modules/parameters"
  environment    = var.environment
  parameter_name = var.job_client_parameter_names.job_spanner_instance_name
  # The real instance name has the env_name as the prefix.
  parameter_value = module.jobdatabase.instance_name
}

module "job_spanner_database_name" {
  count           = var.job_client_parameter_values.job_spanner_database_name == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_client_parameter_names.job_spanner_database_name
  parameter_value = module.jobdatabase.database_name
}

module "job_table_name" {
  count           = var.job_client_parameter_values.job_table_name == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_client_parameter_names.job_table_name
  parameter_value = var.job_client_parameter_values.job_table_name
}

module "job_queue_name" {
  count          = var.job_client_parameter_values.job_queue_name == null ? 0 : 1
  source         = "../../modules/parameters"
  environment    = var.environment
  parameter_name = var.job_client_parameter_names.job_queue_name
  # The real topic name has the env_name as the prefix.
  parameter_value = module.jobqueue.queue_pubsub_topic_name
}

module "read_job_retry_internal" {
  count           = var.job_client_parameter_values.read_job_retry_internal == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_client_parameter_names.read_job_retry_internal
  parameter_value = var.job_client_parameter_values.read_job_retry_internal
}

module "read_job_max_retry_count" {
  count           = var.job_client_parameter_values.read_job_max_retry_count == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_client_parameter_names.read_job_max_retry_count
  parameter_value = var.job_client_parameter_values.read_job_max_retry_count
}
##### JobClient parameters end #####

##### CryptoClient parameters start #####
module "crypto_client_hpke_kem" {
  count           = var.crypto_client_parameter_values.hpke_kem == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.crypto_client_parameter_names.hpke_kem
  parameter_value = var.crypto_client_parameter_values.hpke_kem
}

module "crypto_client_hpke_kdf" {
  count           = var.crypto_client_parameter_values.hpke_kdf == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.crypto_client_parameter_names.hpke_kdf
  parameter_value = var.crypto_client_parameter_values.hpke_kdf
}

module "crypto_client_hpke_aead" {
  count           = var.crypto_client_parameter_values.hpke_aead == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.crypto_client_parameter_names.hpke_aead
  parameter_value = var.crypto_client_parameter_values.hpke_aead
}
##### CryptoClient parameters end #####

##### AutoScalingClient parameters start #####
module "instance_spanner_instance_name" {
  count          = var.job_client_parameter_values.job_spanner_instance_name == null ? 0 : 1
  source         = "../../modules/parameters"
  environment    = var.environment
  parameter_name = var.auto_scaling_client_parameter_names.instance_spanner_instance_name
  # The real instance name has the env_name as the prefix, so fetch from the jobdatabase module.
  parameter_value = module.jobdatabase.instance_name
}

module "instance_spanner_database_name" {
  count           = var.job_client_parameter_values.job_spanner_database_name == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.auto_scaling_client_parameter_names.instance_spanner_database_name
  parameter_value = module.jobdatabase.database_name
}

module "instance_table_name" {
  count           = var.auto_scaling_client_parameter_values.instance_table_name == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.auto_scaling_client_parameter_names.instance_table_name
  parameter_value = var.auto_scaling_client_parameter_values.instance_table_name
}

module "scale_in_hook_name" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.auto_scaling_client_parameter_names.scale_in_hook_name
  parameter_value = module.autoscaling.worker_managed_instance_group_name
}
##### AutoScalingClient parameters end #####

##### MetricClient parameters start #####
module "enable_batch_recording" {
  count           = var.metric_client_parameter_values.enable_batch_recording == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.metric_client_parameter_names.enable_batch_recording
  parameter_value = var.metric_client_parameter_values.enable_batch_recording
}
module "namespace_for_batch_recording" {
  count           = var.metric_client_parameter_values.namespace_for_batch_recording == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.metric_client_parameter_names.namespace_for_batch_recording
  parameter_value = var.metric_client_parameter_values.namespace_for_batch_recording
}
module "batch_recording_time_duration_in_ms" {
  count           = var.metric_client_parameter_values.batch_recording_time_duration_in_ms == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.metric_client_parameter_names.batch_recording_time_duration_in_ms
  parameter_value = var.metric_client_parameter_values.batch_recording_time_duration_in_ms
}
##### MetricClient parameters end #####
