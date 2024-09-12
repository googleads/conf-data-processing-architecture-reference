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

provider "aws" {
  region = var.region
}

data "aws_caller_identity" "current" {}

module "operator_alarm_sns_topic" {
  count  = local.any_alarms_enabled ? 1 : 0
  source = "../../modules/alarmsnstopic"

  environment              = var.environment
  alarm_notification_email = var.alarm_notification_email
  region                   = var.region
}

locals {
  any_alarms_enabled = var.alarms_enabled || var.custom_metrics_alarms_enabled
  sns_topic_arn      = local.any_alarms_enabled ? module.operator_alarm_sns_topic[0].operator_alarm_sns_topic_arn : ""
}

module "job_db" {
  source           = "../../modules/database"
  environment      = var.environment
  table_name       = "${var.environment}-${var.job_client_parameter_values.job_table_name}"
  primary_key      = "JobId"
  primary_key_type = "S"
  service_tag      = "cc-operator-service"
  role_tag         = "job_db"

  read_capacity  = var.job_db_read_capacity
  write_capacity = var.job_db_write_capacity

  #Alarms
  alarms_enabled                             = var.alarms_enabled
  sns_topic_arn                              = local.sns_topic_arn
  read_alarm_name                            = "job_db_table_read_capacity_ratio_alarm"
  write_alarm_name                           = "job_db_table_write_capacity_ratio_alarm"
  eval_period_sec                            = var.job_db_alarm_eval_period_sec
  read_capacity_usage_ratio_alarm_threshold  = var.job_db_read_capacity_usage_ratio_alarm_threshold
  write_capacity_usage_ratio_alarm_threshold = var.job_db_write_capacity_usage_ratio_alarm_threshold
}

module "job_queue" {
  source      = "../../modules/queue"
  environment = var.environment
  region      = var.region
  queue_name  = "${var.environment}-${var.job_client_parameter_values.job_queue_name}"
  service_tag = "cc-operator-service"
  role_tag    = "jobqueue"

  #Alarms
  alarms_enabled                  = var.alarms_enabled
  sns_topic_arn                   = local.sns_topic_arn
  alarm_name                      = "job_queue_old_message_alarm"
  queue_old_message_threshold_sec = var.job_queue_old_message_threshold_sec
  queue_alarm_eval_period_sec     = var.job_queue_alarm_eval_period_sec
}

module "worker_service" {
  source = "../../modules/worker"

  region            = var.region
  instance_type     = var.instance_type
  ec2_iam_role_name = "${var.environment}-WorkerRole"

  worker_template_name = "${var.environment}-launch-template"

  ami_name   = var.ami_name
  ami_owners = var.ami_owners

  enclave_cpu_count  = var.enclave_cpu_count
  enclave_memory_mib = var.enclave_memory_mib

  environment       = var.environment
  ec2_instance_name = "${var.environment}-instance"

  metadata_db_table_arn         = module.job_db.db_arn
  job_queue_arn                 = module.job_queue.queue_sqs_arn
  coordinator_a_assume_role_arn = var.coordinator_a_assume_role_parameter
  coordinator_b_assume_role_arn = var.coordinator_b_assume_role_parameter

  asg_instances_table_arn = ""
  worker_ssh_public_key   = var.worker_ssh_public_key

  # VPC
  worker_security_group_ids = var.enable_user_provided_vpc ? var.user_provided_vpc_security_group_ids : [module.vpc[0].allow_internal_ingress_sg_id, module.vpc[0].allow_egress_sg_id]
  dynamodb_vpc_endpoint_id  = module.vpc[0].dynamodb_vpc_endpoint_id
  s3_vpc_endpoint_id        = module.vpc[0].s3_vpc_endpoint_id

  #Alarms
  # Worker only has alarms setup for custom metrics.
  worker_alarms_enabled            = var.custom_metrics_alarms_enabled
  operator_sns_topic_arn           = local.sns_topic_arn
  job_client_error_threshold       = var.job_client_error_threshold
  job_validation_failure_threshold = var.job_validation_failure_threshold
  worker_job_error_threshold       = var.worker_job_error_threshold
  worker_alarm_eval_period_sec     = var.worker_alarm_eval_period_sec
  worker_alarm_metric_dimensions   = var.worker_alarm_metric_dimensions
}

module "worker_autoscaling" {
  source = "../../modules/cc_autoscaling"

  environment             = var.environment
  region                  = var.region
  asg_name                = "${var.environment}-asg"
  worker_template_id      = module.worker_service.worker_template_id
  worker_template_version = module.worker_service.worker_template_version
  worker_subnet_ids       = var.enable_user_provided_vpc ? var.user_provided_vpc_subnet_ids : module.vpc[0].private_subnet_ids

  enable_autoscaling             = true
  initial_capacity_ec2_instances = var.initial_capacity_ec2_instances
  min_ec2_instances              = var.min_capacity_ec2_instances
  max_ec2_instances              = var.max_capacity_ec2_instances

  jobqueue_sqs_url = module.job_queue.queue_sqs_url
  jobqueue_sqs_arn = module.job_queue.queue_sqs_arn

  lambda_package_storage_bucket_prefix = "${var.environment}-bucket-"

  #Alarms
  alarms_enabled                    = var.alarms_enabled
  sns_topic_arn                     = local.sns_topic_arn
  asg_max_instances_alarm_ratio     = var.asg_max_instances_alarm_ratio
  autoscaling_alarm_eval_period_sec = var.autoscaling_alarm_eval_period_sec
}

module "vpc" {
  count  = var.enable_user_provided_vpc ? 0 : 1
  source = "../../modules/vpc"

  environment            = var.environment
  vpc_cidr               = var.vpc_cidr
  vpc_availability_zones = var.vpc_availability_zones

  lambda_execution_role_ids = []

  dynamodb_allowed_principal_arns = [module.worker_service.worker_enclave_role_arn]
  dynamodb_arns                   = [module.job_db.db_arn]

  s3_allowed_principal_arns = [module.worker_service.worker_enclave_role_arn]
}

module "worker_dashboard" {
  source = "../../modules/workerdashboard"

  environment                   = var.environment
  region                        = var.region
  custom_metrics_alarms_enabled = var.custom_metrics_alarms_enabled
}
