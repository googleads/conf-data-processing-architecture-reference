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

variable "service" {
  description = "Service name for CMRT service."
  type        = string
  default     = "cmrt-service"
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "region" {
  description = "AWS region to deploy components"
  type        = string
}

variable "asg_name" {
  type        = string
  default     = "cmrt-service-workers"
  description = "The name of auto scaling group."
}

variable "enable_autoscaling" {
  type    = bool
  default = true
}

variable "initial_capacity_ec2_instances" {
  description = "Initial capacity for ec2 instances. If autoscaling is not enabled, the number of instances will always equal this value."
  type        = number
  default     = 1
}

variable "worker_template_id" {
  type        = string
  description = "The worker template id to associate with the autoscaling group."
}

variable "worker_template_version" {
  type        = string
  description = "The worker template version to associate with the autoscaling group."
}

variable "max_ec2_instances" {
  description = "Upper bound for autoscaling for ec2 instances."
  type        = number
  default     = 20
}

variable "min_ec2_instances" {
  description = "Lower bound for autoscaling for ec2 instances."
  type        = number
  default     = 1
}

variable "lambda_package_storage_bucket_prefix" {
  type        = string
  description = "Prefix of the bucket to create and store lambda jars in"
}

variable "jobqueue_sqs_url" {
  type        = string
  description = "The URL of the SQS Queue to use as the JobQueue"
}

variable "jobqueue_sqs_arn" {
  type        = string
  description = "The resource arn of the SQS Queue to use as the JobQueue"
}

variable "worker_scaling_ratio" {
  type        = number
  default     = 1
  description = "The ratio of worker instances to jobs (0.5: 1 worker / 2 jobs)"
  validation {
    condition     = var.worker_scaling_ratio > 0
    error_message = "Must be greater than 0."
  }
}

variable "worker_subnet_ids" {
  type        = list(string)
  description = "The subnet ids to launch instances in for the autoscaling group"
}

variable "alarms_enabled" {
  type        = string
  description = "Enable alarms for auto-scaling"
}

variable "sns_topic_arn" {
  type        = string
  description = "SNS topic ARN to forward alerts to"
}

variable "asg_max_instances_alarm_ratio" {
  type        = number
  description = "Ratio of the auto scaling group max instances that should alarm on."
  # default     = 0.9
}

variable "autoscaling_alarm_eval_period_sec" {
  type        = string
  description = "Amount of time (in seconds) for alarm evaluation. Default 60s"
  # default     = "60"
}
