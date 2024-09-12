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

##### JobClient parameters start #####
module "job_table_name" {
  count           = var.job_client_parameter_values.job_table_name == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_client_parameter_names.job_table_name
  parameter_value = var.job_client_parameter_values.job_table_name
}

module "job_queue_name" {
  count           = var.job_client_parameter_values.job_queue_name == null ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.job_client_parameter_names.job_queue_name
  parameter_value = module.job_queue.queue_sqs_url
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
module "scale_in_hook_name" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = var.auto_scaling_client_parameter_names.scale_in_hook_name
  parameter_value = module.worker_autoscaling.scale_in_hook_name
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
