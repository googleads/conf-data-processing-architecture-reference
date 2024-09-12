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


################################################################################
# Global Variables.
################################################################################

variable "environment" {
  description = "Environment where this service is deployed (e.g. dev, prod)."
  type        = string
}

variable "project_id" {
  description = "project id"
  type        = string
}

################################################################################
# Network Variables.
################################################################################

variable "network" {
  description = "VPC Network name or self-link to use for worker."
  type        = string
}

variable "egress_internet_tag" {
  description = "Instance tag that grants internet access."
  type        = string
}

################################################################################
# Worker Variables.
################################################################################

variable "instance_type" {
  description = "GCE instance type for worker."
  type        = string
}

variable "instance_disk_image" {
  description = "The image from which to initialize the worker instance disk."
  type        = string
}

variable "worker_instance_disk_type" {
  description = "The worker instance disk type."
  type        = string
}

variable "worker_instance_disk_size_gb" {
  description = "The size of the worker instance disk image in GB."
  type        = number
}

variable "worker_logging_enabled" {
  description = "Whether to enable worker logging."
  type        = bool
}

variable "worker_monitoring_enabled" {
  description = "Whether to enable worker monitoring."
  type        = bool
}

variable "worker_memory_monitoring_enabled" {
  description = "Whether to enable worker memory monitoring."
  type        = bool
}

variable "worker_container_log_redirect" {
  description = "Where to output container logs: cloud_logging, serial, true (both), false (neither)."
  type        = string
}

variable "worker_image" {
  description = "The worker docker image."
  type        = string
}

variable "worker_image_signature_repos" {
  description = <<-EOT
    A list of comma-separated container repositories that store the worker image signatures
    that are generated by Sigstore Cosign.
    Example: "us-docker.pkg.dev/projectA/repo/example,us-docker.pkg.dev/projectB/repo/example".
  EOT
  type        = string
  default     = ""
}

variable "worker_restart_policy" {
  description = "The TEE restart policy. Currently only supports Never"
  type        = string
}

variable "allowed_operator_service_account" {
  description = "The service account provided by coordinator for operator worker to impersonate."
  type        = string
}

variable "metadatadb_name" {
  description = "Name of the JobMetadata Spanner database."
  type        = string
}

variable "metadatadb_instance_name" {
  description = "Name of the TerminatedInstances Spanner instance."
  type        = string
}

variable "job_queue_sub" {
  description = "Name of the job queue subscription."
  type        = string
}

variable "job_queue_topic" {
  description = "Name of the job queue topic."
  type        = string
}

variable "user_provided_worker_sa_email" {
  description = "User provided service account email for worker."
  type        = string
}

variable "worker_instance_force_replace" {
  description = "Whether to force worker instance replacement for every deployment"
  type        = bool
}

################################################################################
# Monitoring Variables.
################################################################################

variable "autoscaler_cloudfunction_name" {
  description = "Name for the cloud function used in autoscaling worker VMs."
  type        = string
}

variable "autoscaler_name" {
  description = "Name of the autoscaler for the worker VM."
  type        = string
}

variable "vm_instance_group_name" {
  description = "Name for the instance group for the worker VM."
  type        = string
}

variable "alarms_enabled" {
  description = "Enable alarms for this service."
  type        = bool
}

variable "alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = string
}

variable "notification_channel_id" {
  description = "Notification channel to which to send alarms."
  type        = string
}
