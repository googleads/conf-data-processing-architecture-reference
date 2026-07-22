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

terraform {
  required_providers {
    google = {
      source  = "hashicorp/google"
      version = ">= 4.36"
    }
  }
}

module "otel_collector" {
  source      = "../../modules/opentelemetry_collector"
  environment = var.environment
  project_id  = var.project_id
  network     = var.network

  internet_tag_for_otel = var.internet_tag_for_otel

  subnets_per_region        = var.subnets_per_region
  collector_regional_config = var.collector_regional_config

  user_provided_collector_sa_email = var.user_provided_collector_sa_email
  collector_instance_type          = var.collector_instance_type
  collector_service_port           = var.collector_service_port
  collector_service_port_name      = var.collector_service_port_name
  collector_min_instance_ready_sec = var.collector_min_instance_ready_sec
  collector_startup_script = templatefile("../../modules/opentelemetry_collector/collector_startup.tftpl", {
    otel_collector_image_uri = var.otel_collector_startup_config.otel_collector_image_uri
    metric_prefix            = var.otel_collector_startup_config.metric_prefix
    send_batch_max_size      = var.otel_collector_startup_config.send_batch_max_size
    send_batch_size          = var.otel_collector_startup_config.send_batch_size
    send_batch_timeout       = var.otel_collector_startup_config.send_batch_timeout
    collector_queue_size     = var.otel_collector_startup_config.collector_queue_size
    http_receiver_port       = var.collector_service_port
  })

  collector_exceed_cpu_usage_alarm         = var.collector_exceed_cpu_usage_alarm
  collector_exceed_memory_usage_alarm      = var.collector_exceed_memory_usage_alarm
  collector_startup_error_alarm            = var.collector_startup_error_alarm
  collector_crash_error_alarm              = var.collector_crash_error_alarm
  export_metric_to_collector_error_alarm   = var.export_metric_to_collector_error_alarm
  collector_queue_size_alarm               = var.collector_queue_size_alarm
  collector_send_metric_failure_rate_alarm = var.collector_send_metric_failure_rate_alarm
  collector_refuse_metric_rate_alarm       = var.collector_refuse_metric_rate_alarm
}

module "otel_collector_load_balancer" {
  source                        = "../../modules/otel_load_balancer"
  environment                   = var.environment
  project_id                    = var.project_id
  network                       = var.network
  subnets_per_region            = var.subnets_per_region
  proxy_only_subnets_per_region = var.proxy_only_subnets_per_region
  instance_groups               = module.otel_collector.collector_instance_groups
  service_name                  = var.collector_service_name
  service_port_name             = var.collector_service_port_name
  service_port                  = var.collector_service_port
  dns_name                      = var.collector_dns_name
  domain_name                   = var.collector_domain_name
}