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
# Variables for parameter names and values.
# For some parameters, the real value may be added with the env_name prefix
# in the terraform.
################################################################################

variable "common_parameter_names" {
  description = "Parameter names shared in CPIO."
  type = object({
    log_option                = string
    enabled_log_levels        = string
    cpu_thread_count          = string
    cpu_thread_pool_queue_cap = string
    io_thread_count           = string
    io_thread_pool_queue_cap  = string
  })
  default = {
    log_option                = "CMRT_COMMON_LOG_OPTION"
    enabled_log_levels        = "CMRT_COMMON_ENABLED_LOG_LEVELS"
    cpu_thread_count          = "CMRT_COMMON_CPU_THREAD_COUNT"
    cpu_thread_pool_queue_cap = "CMRT_COMMON_CPU_THREAD_POOL_QUEUE_CAP"
    io_thread_count           = "CMRT_COMMON_IO_THREAD_COUNT"
    io_thread_pool_queue_cap  = "CMRT_COMMON_IO_THREAD_POOL_QUEUE_CAP"
  }
}

variable "common_parameter_values" {
  description = "Parameter value shared in CPIO."
  type = object({
    log_option                = string
    enabled_log_levels        = string
    cpu_thread_count          = string
    cpu_thread_pool_queue_cap = string
    io_thread_count           = string
    io_thread_pool_queue_cap  = string
  })
  default = {
    log_option                = null
    enabled_log_levels        = null
    cpu_thread_count          = null
    cpu_thread_pool_queue_cap = null
    io_thread_count           = null
    io_thread_pool_queue_cap  = null
  }
}

variable "job_lifecycle_helper_parameter_names" {
  description = "Parameter names for job lifecycle helper."
  type = object({
    job_lifecycle_helper_retry_limit                     = string
    job_lifecycle_helper_visibility_timeout_extend_time  = string
    job_lifecycle_helper_job_processing_timeout          = string
    job_lifecycle_helper_job_extending_worker_sleep_time = string
    job_lifecycle_helper_enable_metric_recording         = string
    job_lifecycle_helper_metric_namespace                = string
  })
  default = {
    job_lifecycle_helper_retry_limit                     = "CMRT_JOB_LIFECYCLE_HELPER_RETRY_LIMIT"
    job_lifecycle_helper_visibility_timeout_extend_time  = "CMRT_JOB_LIFECYCLE_HELPER_VISIBILITY_TIMEOUT_EXTEND_TIME"
    job_lifecycle_helper_job_processing_timeout          = "CMRT_JOB_LIFECYCLE_HELPER_JOB_PROCESSING_TIMEOUT"
    job_lifecycle_helper_job_extending_worker_sleep_time = "CMRT_JOB_LIFECYCLE_HELPER_JOB_EXTENDING_WORKER_SLEEP_TIME"
    job_lifecycle_helper_enable_metric_recording         = "CMRT_JOB_LIFECYCLE_HELPER_JOB_ENABLE_METRIC_RECORDING"
    job_lifecycle_helper_metric_namespace                = "CMRT_JOB_LIFECYCLE_HELPER_JOB_METRIC_NAMESPACE"
  }
}

variable "job_lifecycle_helper_parameter_values" {
  description = "Parameter value for job lifecycle helper."
  type = object({
    job_lifecycle_helper_retry_limit                     = string
    job_lifecycle_helper_visibility_timeout_extend_time  = string
    job_lifecycle_helper_job_processing_timeout          = string
    job_lifecycle_helper_job_extending_worker_sleep_time = string
    job_lifecycle_helper_enable_metric_recording         = bool
    job_lifecycle_helper_metric_namespace                = string
  })
  default = {
    job_lifecycle_helper_retry_limit                     = null
    job_lifecycle_helper_visibility_timeout_extend_time  = null
    job_lifecycle_helper_job_processing_timeout          = null
    job_lifecycle_helper_job_extending_worker_sleep_time = null
    job_lifecycle_helper_enable_metric_recording         = null
    job_lifecycle_helper_metric_namespace                = null
  }
}

variable "job_client_parameter_names" {
  description = "Parameter names for job client."
  type = object({
    job_queue_name            = string
    job_table_name            = string
    job_spanner_instance_name = string
    job_spanner_database_name = string
    read_job_retry_internal   = string
    read_job_max_retry_count  = string
  })
  default = {
    job_queue_name            = "CMRT_JOB_CLIENT_JOB_QUEUE_NAME"
    job_table_name            = "CMRT_JOB_CLIENT_JOB_TABLE_NAME"
    job_spanner_instance_name = "CMRT_GCP_JOB_CLIENT_SPANNER_INSTANCE_NAME"
    job_spanner_database_name = "CMRT_GCP_JOB_CLIENT_SPANNER_DATABASE_NAME"
    read_job_retry_internal   = "CMRT_JOB_CLIENT_READ_JOB_RETRY_INTERVAL"
    read_job_max_retry_count  = "CMRT_JOB_CLIENT_READ_JOB_MAX_RETRY_COUNT"
  }
}

variable "job_client_parameter_values" {
  description = "Parameter value for job client."
  type = object({
    job_queue_name            = string
    job_table_name            = string
    job_spanner_instance_name = string
    job_spanner_database_name = string
    read_job_retry_internal   = string
    read_job_max_retry_count  = string
  })
  default = {
    job_queue_name            = "JobQueue"
    job_table_name            = "JobTable"
    job_spanner_instance_name = "job-spanner"
    job_spanner_database_name = "job-database"
    read_job_retry_internal   = null
    read_job_max_retry_count  = null
  }
}

variable "crypto_client_parameter_names" {
  description = "Parameter names for crypto client."
  type = object({
    hpke_kem  = string
    hpke_kdf  = string
    hpke_aead = string
  })
  default = {
    hpke_kem  = "CMRT_CRYPTO_CLIENT_HPKE_KEM"
    hpke_kdf  = "CMRT_CRYPTO_CLIENT_HPKE_KDF"
    hpke_aead = "CMRT_CRYPTO_CLIENT_HPKE_AEAD"
  }
}

variable "crypto_client_parameter_values" {
  description = "Parameter value for crypto client."
  type = object({
    hpke_kem  = string
    hpke_kdf  = string
    hpke_aead = string
  })
  default = {
    hpke_kem  = null
    hpke_kdf  = null
    hpke_aead = null
  }
}

variable "auto_scaling_client_parameter_names" {
  description = "Parameter names for auto-scaling client."
  type = object({
    scale_in_hook_name             = string
    instance_table_name            = string
    instance_spanner_instance_name = string
    instance_spanner_database_name = string
  })
  default = {
    scale_in_hook_name             = "CMRT_AUTO_SCALING_CLIENT_SCALE_IN_HOOK_NAME"
    instance_table_name            = "CMRT_AUTO_SCALING_CLIENT_INSTANCE_TABLE_NAME"
    instance_spanner_instance_name = "CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_INSTANCE_NAME"
    instance_spanner_database_name = "CMRT_GCP_AUTO_SCALING_CLIENT_SPANNER_DATABASE_NAME"
  }
}

# instance_group_name will be generated automatically in AutoScaling module, so we don't need to pass in through variable.
# Instance table and job table are sharing the same Spanner and database, so we don't need to pass in through variables.
variable "auto_scaling_client_parameter_values" {
  description = "Parameter value for auto-scaling client."
  type = object({
    instance_table_name = string
  })
  default = {
    instance_table_name = "AsgInstances"
  }
}

variable "metric_client_parameter_names" {
  description = "Parameter names for metric client."
  type = object({
    enable_batch_recording              = string
    namespace_for_batch_recording       = string
    batch_recording_time_duration_in_ms = string
  })
  default = {
    enable_batch_recording              = "CMRT_METRIC_CLIENT_ENABLE_BATCH_RECORDING"
    namespace_for_batch_recording       = "CMRT_METRIC_CLIENT_NAMESPACE_FOR_BATCH_RECORDING"
    batch_recording_time_duration_in_ms = "CMRT_METRIC_CLIENT_BATCH_RECORDING_TIME_DURATION_IN_MS"
  }
}

variable "metric_client_parameter_values" {
  description = "Parameter values for metric client."
  type = object({
    enable_batch_recording              = bool
    namespace_for_batch_recording       = string
    batch_recording_time_duration_in_ms = string
  })
  default = {
    enable_batch_recording              = null
    namespace_for_batch_recording       = null
    batch_recording_time_duration_in_ms = null
  }
}
