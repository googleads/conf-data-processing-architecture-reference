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

variable "environment" {
  description = "Name of the environment. e.g. prod or dev."
  type        = string
  nullable    = false
}

variable "function_name" {
  description = "The name of the cloud function. e.g. function-handler. Must be unique."
  type        = string
  nullable    = false
}

variable "region" {
  description = "The GCP region to deploy the function."
  type        = string
  nullable    = false
}

variable "description" {
  description = "The description to show for this cloud function."
  type        = string
  nullable    = false
}

variable "function_entrypoint" {
  description = "The entypoint - package and class name of the function handler."
  type        = string
  nullable    = false
}

variable "source_bucket_name" {
  description = "Bucket where the zipped jar for the function source will be read from."
  type        = string
  nullable    = false
}

variable "cloud_function_jar" {
  description = "Path of the zipped jar with the cloud function source."
  type        = string
  nullable    = false
}

variable "min_instance_count" {
  description = "Minimum number of instances in the cloud function pool."
  type        = number
  nullable    = false
}

variable "max_instance_count" {
  description = "Maximum number of instances in the cloud function pool."
  type        = number
  nullable    = false
}

variable "concurrency" {
  description = "How many concurrent executions each instance can handle."
  type        = number
  nullable    = false
}

variable "cpu_count" {
  description = "How many CPUs to give to each instance. e.g. 0.167 or 1."
  type        = string
  nullable    = false

}

variable "memory_mb" {
  description = "How much memory in MB to give to each instance. e.g. 256."
  type        = number
  nullable    = false
}

variable "trigger_pubsub_id" {
  description = "PubSub ID whose messages trigger this function. e.g. projects/project/topics/topic."
  type        = string
  nullable    = false
}

variable "trigger_pubsub_retry_policy" {
  description = "The retry policy for the trigger from PubSub. One of: [RETRY_POLICY_UNSPECIFIED, RETRY_POLICY_DO_NOT_RETRY, RETRY_POLICY_RETRY]"
  type        = string
  nullable    = false
}

variable "runtime_cloud_function_service_account_email" {
  description = "Pre-existing service account to use for runtime cloud function."
  type        = string
  nullable    = false
}

variable "event_trigger_service_account_email" {
  description = "Pre-existing service account to use for event trigger."
  type        = string
  nullable    = false
}

variable "vpc_connector_id" {
  description = "Serverless VPC Access connector ID to use for all egress traffic."
  type        = string
  default     = null
}
