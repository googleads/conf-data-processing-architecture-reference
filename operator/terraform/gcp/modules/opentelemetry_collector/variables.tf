/**
 * Copyright 2025 Google LLC
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

#################################################################################
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

################################################################################
# Collector Variables.
################################################################################
variable "collector_regional_config" {
  description = "Region and zone level configurations."
  type = map(object({
    zonal_config = map(object({
      min_collector_count              = optional(number, 1)
      max_collector_count              = optional(number, 3)
      collector_cpu_utilization_target = optional(number, 0.8)
    }))
  }))
  default = {}
}

variable "user_provided_collector_sa_email" {
  description = "User provided service account email for OpenTelemetry Collector."
  type        = string
}

variable "collector_instance_type" {
  description = "GCE instance type for worker."
  type        = string
}

variable "cos_image_family" {
  description = "COS image family to use."
  type        = string
}

variable "collector_min_instance_ready_sec" {
  description = "Waiting time for the new instance to be ready."
  type        = number
  default     = 240
}

variable "collector_startup_script" {
  description = "Script to configure and start the otel collector."
  type        = string
}

variable "collector_service_port" {
  description = "The port that receives HTTP otlp traffic destined for the OpenTelemetry collector."
  type        = number
}

variable "collector_service_port_name" {
  description = "The name of the http port that receives traffic destined for the OpenTelemetry collector."
  type        = string
}

variable "internet_tag_for_otel" {
  description = "Instance tag that grants internet access to the otel instances. This tag should be present in the route table for the VPC network where the otel collector and server are deployed in."
  type        = string
}

################################################################################
# Regional Variables.
################################################################################

variable "subnets_per_region" {
  description = "Subnet for server."
  type        = map(string)
}

################################################################################
# Alarm Variables.
################################################################################

variable "collector_exceed_cpu_usage_alarm" {
  description = "Configuration for the exceed CPU usage alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
}

variable "collector_exceed_memory_usage_alarm" {
  description = "Configuration for the exceed memory usage alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
}

variable "collector_startup_error_alarm" {
  description = "Configuration for the collector startup error alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
}

variable "collector_crash_error_alarm" {
  description = "Configuration for the collector crash error alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
}

variable "export_metric_to_collector_error_alarm" {
  description = "Configuration for the server exporting metrics error alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
}

variable "collector_queue_size_alarm" {
  description = "Configuration for the collector queue size alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
}

variable "collector_send_metric_failure_rate_alarm" {
  description = "Configuration for the collector send metric failure alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
}

variable "collector_refuse_metric_rate_alarm" {
  description = "Configuration for the collector refuse metric alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
}
