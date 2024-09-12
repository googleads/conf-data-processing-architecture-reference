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

variable "environment" {
  description = "Environment name."
  type        = string
}

variable "region" {
  description = "AWS region to deploy service."
  type        = string
}

variable "instance_type" {
  description = "Parent EC2 instance type."
  type        = string
  default     = "m5.xlarge" # 4 cores, 16GiB
}

variable "enclave_cpu_count" {
  description = "Number of CPUs to allocate to the enclave."
  type        = number
  default     = 2 # Leave 2 vCPUs to host OS (minimum required).  Depends on instance_type selected.
}

variable "enclave_memory_mib" {
  description = "Memory (in mebibytes) to allocate to the enclave."
  type        = number
  default     = 13312 # 13 GiB. ~15GiB are available on m5.xlarge, leave 2GiB for host OS.  Depends on instance_type selected.
}

variable "ami_name" {
  description = "AMI name."
  type        = string
}

variable "ami_owners" {
  type        = list(string)
  description = "AWS accounts to check for worker AMIs."
  default     = ["self"]
}

variable "initial_capacity_ec2_instances" {
  description = "Autoscaling initial capacity."
  type        = number
  default     = 2
}

variable "min_capacity_ec2_instances" {
  description = "Autoscaling min capacity."
  type        = number
  default     = 1
}

variable "max_capacity_ec2_instances" {
  description = "Autoscaling max capacity."
  type        = number
  default     = 20
}


variable "coordinator_a_assume_role_parameter" {
  type        = string
  description = "ARN of the role that the workers should assume to access coordinator A."
  # TODO: Remove after migration to updated client.
  default = ""
}

variable "coordinator_b_assume_role_parameter" {
  type        = string
  description = "ARN of the role that the workers should assume to access coordinator B."
  default     = ""
}

variable "worker_ssh_public_key" {
  description = "RSA public key to be used for SSH access to worker EC2 instance."
  type        = string
  default     = ""
}

################################################################################
# JobDB Variables
################################################################################

variable "job_db_read_capacity" {
  type        = number
  description = "The read capacity units for the Job DynamoDB Table"
  default     = 5
}

variable "job_db_write_capacity" {
  type        = number
  description = "The write capacity units for the Job DynamoDB Table"
  default     = 5
}

################################################################################
# VPC, subnets and security group variables
################################################################################

variable "enable_user_provided_vpc" {
  description = "Enable override of default VPC with an user-provided VPC."
  type        = bool
  default     = false
}

variable "user_provided_vpc_subnet_ids" {
  description = "Ids of the user-provided VPC subnets."
  type        = list(string)
  default     = []
}

variable "user_provided_vpc_security_group_ids" {
  description = "Ids of the user-provided VPC security groups."
  type        = list(string)
  default     = []
}

variable "vpc_cidr" {
  description = "VPC CIDR range for the secure VPC."
  type        = string
  default     = "10.0.0.0/16"
}

variable "vpc_availability_zones" {
  description = "Specify the letter identifiers of which availability zones to deploy resources, such as a, b or c."
  type        = set(string)
  default = [
    "a",
    "b",
    "c",
    "d",
    "e",
  ]
}

################################################################################
# Shared Alarm Variables
################################################################################

variable "alarms_enabled" {
  type        = string
  description = "Enable alarms for worker (includes alarms for autoscaling/jobqueue/worker)"
  default     = true
}

variable "custom_metrics_alarms_enabled" {
  type        = string
  description = "Enable alarms based on custom metrics"
  default     = false
}

variable "alarm_notification_email" {
  description = "Email to send operator component alarm notifications"
  type        = string
  default     = "noreply@example.com"
}

################################################################################
# Job Queue Alarm Variables
################################################################################

variable "job_queue_old_message_threshold_sec" {
  type        = number
  description = "Alarm threshold for old job queue messages in seconds."
  default     = 3600 //one hour
}

variable "job_queue_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Default 300s"
  type        = string
  default     = "300"
}

################################################################################
# Job Table Alarm Variables
################################################################################

variable "job_db_alarm_eval_period_sec" {
  type        = string
  description = "Amount of time (in seconds) for alarm evaluation. Default 60"
  default     = "60"
}

variable "job_db_read_capacity_usage_ratio_alarm_threshold" {
  type        = string
  description = "Read capacity usage ratio greater than this to send alarm. Value should be decimal. Example: 0.9"
  default     = "0.9"
}

variable "job_db_write_capacity_usage_ratio_alarm_threshold" {
  type        = string
  description = "Write capacity usage ratio greater than this to send alarm. Value should be decimal. Example: 0.9"
  default     = "0.9"
}

################################################################################
# Worker Alarm Variables
################################################################################

variable "job_client_error_threshold" {
  type        = number
  description = "Alarm threshold for job client errors."
  default     = 0
}

variable "job_validation_failure_threshold" {
  type        = number
  description = "Alarm threshold for job validation failures."
  default     = 0
}

variable "worker_job_error_threshold" {
  type        = number
  description = "Alarm threshold for worker job errors."
  default     = 0
}

variable "worker_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Default 300s"
  type        = string
  default     = "300"
}

variable "worker_alarm_metric_dimensions" {
  description = "Metric dimensions for worker alarms"
  type        = list(string)
  default     = ["JobHandlingError"]
}

################################################################################
# Autoscaling Alarm Variables
################################################################################

variable "asg_max_instances_alarm_ratio" {
  type        = number
  description = "Ratio of the auto scaling group max instances that should alarm on."
  default     = 0.9
}

variable "autoscaling_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Default 60s"
  type        = string
  default     = "60"
}
