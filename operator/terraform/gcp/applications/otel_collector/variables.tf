/**
 * Copyright 2026 Google LLC
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

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production"
  type        = string
}


variable "region_zone" {
  description = "Region zone where all services will be created."
  type        = string
}

################################################################################
# Network Variables.
################################################################################

variable "network" {
  description = "VPC network name or self-link."
  type        = string
}

variable "subnets_per_region" {
  description = "Subnet for server."
  type        = map(string)
  default     = {}
}

variable "proxy_only_subnets_per_region" {
  description = "Proxy-only subnet for server."
  type        = map(string)
  default     = {}
}


################################################################################
# OpenTelemetry Collector variables
################################################################################

variable "internet_tag_for_otel" {
  description = "Instance tag that grants internet access to the otel instances."
  type        = string
  default     = "egress-internet"
}

variable "collector_instance_type" {
  description = "GCE instance type for worker."
  type        = string
  default     = "n2d-standard-2"
}

variable "cos_image_family" {
  description = "COS image family to use."
  type        = string
  default     = "cos-121-lts"
}

variable "user_provided_collector_sa_email" {
  description = "User provided service account email for OpenTelemetry Collector."
  type        = string
  default     = ""
}

variable "collector_service_port_name" {
  description = "The name of the http port that receives traffic destined for the OpenTelemetry collector."
  type        = string
  default     = "otlp"
}

variable "collector_service_port" {
  description = "The value of the http port that receives traffic destined for the OpenTelemetry collector."
  type        = number
  default     = 4318
}

variable "collector_domain_name" {
  description = "The dns domain name for OpenTelemetry collector."
  type        = string
  default     = "collector.metrics"
}

variable "collector_dns_name" {
  description = "The dns name for OpenTelemetry collector."
  type        = string
  default     = "scptestings.dev"
}

variable "otel_collector_startup_config" {
  description = "Startup configuration for the OpenTelemetry collector."
  type = object({
    otel_collector_image_uri = optional(string, "otel/opentelemetry-collector-contrib:0.122.1")
    metric_prefix            = optional(string, "custom.googleapis.com")
    send_batch_max_size      = optional(number, 200)
    send_batch_size          = optional(number, 200)
    send_batch_timeout       = optional(string, "5s")
    collector_queue_size     = optional(number, 5000)
  })
  default = {}
}



variable "collector_min_instance_ready_sec" {
  description = "Waiting time for the new instance to be ready."
  type        = number
  default     = 120
}


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
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 0.9,
    severity : "moderate",
    auto_close_sec : 1800
  }
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
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 6442450944, # 6 GB
    severity : "moderate",
    auto_close_sec : 1800
  }
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
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 50,
    severity : "moderate",
    auto_close_sec : 1800
  }
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
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 50,
    severity : "moderate",
    auto_close_sec : 1800
  }
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
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 50,
    severity : "moderate",
    auto_close_sec : 1800
  }
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
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 0.8,
    severity : "moderate",
    auto_close_sec : 1800
  }
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
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 0.05,
    severity : "moderate",
    auto_close_sec : 1800
  }
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
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 0.05,
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "collector_service_name" {
  description = "The name of the service for the load balancer."
  type        = string
  default     = "collector"
}

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
