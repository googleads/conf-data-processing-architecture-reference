# Copyright 2022 Google LLC
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

terraform {
  required_providers {
    google = {
      source  = "hashicorp/google-beta"
      version = "7.15"
    }
  }
}

provider "google" {
  project = var.project_id
  region  = var.primary_region
  zone    = var.primary_region_zone
}

provider "google" {
  alias   = "domain"
  project = var.project_id
  region  = var.secondary_region
  zone    = var.secondary_region_zone
}

locals {
  private_key_service_addtional_environment = "pre-${var.environment}"

  # Service Domains
  service_subdomain_suffix      = var.service_subdomain_suffix != null ? var.service_subdomain_suffix : "-${var.environment}"
  public_key_domain             = var.environment != "prod" ? "${var.public_key_service_subdomain}${local.service_subdomain_suffix}.${var.parent_domain_name}" : "${var.public_key_service_subdomain}.${var.parent_domain_name}"
  private_key_domain            = "${var.private_key_service_subdomain}${local.service_subdomain_suffix}.${var.parent_domain_name}"
  private_key_domain_additional = "${var.private_key_service_subdomain}-${local.private_key_service_addtional_environment}.${var.parent_domain_name}"
  service_domain_to_address_map = (
    var.private_key_service_addon_container_image_url == ""
    ? {
      (local.public_key_domain) : module.public_key_service.loadbalancer_ip,
      (local.private_key_domain) : module.private_key_service.loadbalancer_ip
    }
    : {
      (local.public_key_domain) : module.public_key_service.loadbalancer_ip,
      (local.private_key_domain) : module.private_key_service.loadbalancer_ip
      (local.private_key_domain_additional) : module.private_key_service_addon[0].loadbalancer_ip
    }
  )

  # Service Regions
  region_map = { for reg in [var.primary_region, var.secondary_region] : reg => reg }
  private_key_service_regions = concat(
    [var.primary_region, var.secondary_region],
    var.private_key_service_additional_regions
  )
  public_key_service_regions = concat(
    [var.primary_region, var.secondary_region],
    var.public_key_service_cr_regions
  )

  key_sets = flatten([
    for key_set in var.key_sets_config.key_sets : key_set.name
  ])

  all_service_accounts = flatten([
    for _, operator in var.allowed_operators : operator.service_accounts
  ])

  # KMS Base URIs
  kms_key_base_uri           = "gcp-kms://${module.key_management_service.kms_key_ring_id}/cryptoKeys/${var.environment}_$setName$_kms_key"
  migration_kms_key_base_uri = "gcp-kms://${module.key_management_service.kms_key_ring_id}/cryptoKeys/${var.environment}_$setName$_kms_key"
  migration_peer_coordinator_kms_key_base_uri = (var.migration_peer_coordinator_kms_key_base_uri == null
    ? var.peer_coordinator_kms_key_base_uri
    : var.migration_peer_coordinator_kms_key_base_uri
  )

  # Key Migration Tool Safety Preconditions
  base_uris_are_different      = (local.kms_key_base_uri != local.migration_kms_key_base_uri)
  peer_base_uris_are_different = (var.peer_coordinator_kms_key_base_uri != local.migration_peer_coordinator_kms_key_base_uri)
  key_migration_tool_safe_to_generate = (var.populate_migration_key_data
    && var.key_migration_tool_container_image_url != null
    && var.key_migration_tool_migrator_mode == "generate"
    && length(var.key_sets_vending_config.allowed_migrators) == 0
    && local.base_uris_are_different
    && local.peer_base_uris_are_different
  )
  key_migration_tool_safe_to_migrate = (var.populate_migration_key_data
    && var.key_migration_tool_container_image_url != null
    && var.key_migration_tool_migrator_mode == "migrate"
    && length(var.key_sets_vending_config.allowed_migrators) > 0
    && !local.base_uris_are_different
    && !local.peer_base_uris_are_different
  )
  key_migration_tool_safe_to_cleanup = (!var.populate_migration_key_data
    && var.key_migration_tool_container_image_url != null
    && var.key_migration_tool_migrator_mode == "cleanup"
    && length(var.key_sets_vending_config.allowed_migrators) == 0
  )
}

resource "null_resource" "has_valid_migration_configuration" {
  # Ensures that if it is desired to populate the migration key data, that a
  # migration peer kek uri is provided and a migration kek uri can be safely
  # constructed.
  lifecycle {
    precondition {
      condition = !var.populate_migration_key_data || (
        var.migration_peer_coordinator_kms_key_base_uri != null
        && var.location_new_key_ring != null
      )
      error_message = <<EOF
Variable populate_migration_key_data is set to true, but the required migration
data is not available. To enable migration, provide values for
'location_new_key_ring' and 'migration_peer_coordinator_kms_key_base_uri'.
EOF
    }
  }
}

resource "null_resource" "can_safely_run_key_migration_tool" {
  # Ensures general safety before running the migration tool.
  # Note: For true safety during 'migrate' mode, ALL keys or customers MUST be
  #       in 'key_sets_vending_config.allowed_migrators'
  lifecycle {
    precondition {
      condition = (!var.run_key_migration_tool
        || local.key_migration_tool_safe_to_generate
        || local.key_migration_tool_safe_to_migrate
        || local.key_migration_tool_safe_to_cleanup
      )
      error_message = <<EOF
Invalid configuration for running the Key Migration Tool. Please check the requirements for your chosen 'key_migration_tool_migrator_mode':
  - 'generate': 'populate_migration_key_data' must be true AND 'key_sets_vending_config.allowed_migrators' must be empty.
  - 'migrate': 'populate_migration_key_data' must be true AND 'key_sets_vending_config.allowed_migrators' must NOT be empty.
  - 'cleanup': 'populate_migration_key_data' must be false AND 'key_sets_vending_config.allowed_migrators' must be empty.
EOF
    }
  }
}

module "alerts_on_quota" {
  source = "../../modules/alert_on_quota"

  project     = var.project_id
  environment = var.environment

  alert_duration_sec        = var.quota_alert_duration_sec
  alert_eval_period_sec     = var.quota_alert_eval_period_sec
  alert_max_over_minutes    = var.quota_alert_max_over_minutes
  alert_threshold_important = var.quota_alert_threshold_important
  alert_threshold_urgent    = var.quota_alert_threshold_urgent
}

module "vpc_new" {
  source = "../../modules/vpc_new"

  environment = var.environment
  project_id  = var.project_id
}

module "vpc_nat" {
  source = "../../modules/vpc_nat"

  for_each    = local.region_map
  project_id  = var.project_id
  environment = var.environment
  region      = each.value
  network     = module.vpc_new.network
}

module "keydb" {
  source                                     = "../../modules/keydb"
  project_id                                 = var.project_id
  environment                                = var.environment
  spanner_instance_config                    = var.spanner_instance_config
  spanner_processing_units                   = var.spanner_processing_units
  custom_configuration_name                  = var.spanner_custom_configuration_name
  custom_configuration_display_name          = var.spanner_custom_configuration_display_name
  custom_configuration_base_config           = var.spanner_custom_configuration_base_config
  custom_configuration_read_replica_location = var.spanner_custom_configuration_read_replica_location
}

module "keygenerationservice" {
  source = "../../modules/keygenerationservice"

  project_id                = var.project_id
  environment               = var.environment
  network                   = module.vpc_new.network
  region                    = var.key_generation_region
  allow_stopping_for_update = var.key_generation_allow_stopping_for_update
  egress_internet_tag       = module.vpc_new.egress_internet_tag

  # Data args
  container_image_url               = var.key_generation_service_container_image_url
  spanner_database_name             = module.keydb.keydb_name
  spanner_instance_name             = module.keydb.keydb_instance_name
  key_generation_logging_enabled    = var.key_generation_logging_enabled
  key_generation_monitoring_enabled = var.key_generation_monitoring_enabled

  # Business Rules Args
  key_generation_cron_schedule  = var.key_generation_cron_schedule
  key_generation_cron_time_zone = var.key_generation_cron_time_zone
  key_generation_tee_allowed_sa = var.key_generation_tee_allowed_sa

  # TEE Args
  instance_disk_image               = var.instance_disk_image
  multiparty                        = true
  key_generation_tee_restart_policy = var.key_generation_tee_restart_policy

  # Monitoring Args
  alarms_enabled                           = var.alarms_enabled
  keydb_instance_name                      = module.keydb.keydb_instance_name
  key_generation_alignment_period          = var.key_generation_alignment_period
  single_keyset_alignment_periods          = var.key_generation_single_keyset_alignment_periods
  create_alert_alignment_periods           = var.key_generation_create_alert_alignment_periods
  undelivered_messages_threshold           = var.key_generation_undelivered_messages_threshold
  key_generation_error_threshold           = var.key_generation_error_threshold
  key_generation_precheck_error_threshold  = var.key_generation_precheck_error_threshold
  key_generation_precheck_alignment_period = var.key_generation_precheck_alignment_period

  # An update to any of these variables will trigger the keygen instance for replacement.
  key_gen_secrets_hash = sha256(jsonencode({
    kms_key_base_uri                            = local.kms_key_base_uri
    migration_kms_key_base_uri                  = local.migration_kms_key_base_uri
    peer_coordinator_kms_key_base_uri           = var.peer_coordinator_kms_key_base_uri
    migration_peer_coordinator_kms_key_base_uri = local.migration_peer_coordinator_kms_key_base_uri
  }))
}

### KMS
## Current Key Ring
# Key set acl kek pool
module "key_set_acl_kek_pool" {
  for_each = var.allowed_operators
  source   = "../../modules/allowed_operator_pool"

  environment      = var.environment
  key_sets         = toset(each.value.key_sets)
  key_ring_id      = module.key_management_service.kms_key_ring_id
  allowed_operator = each.value
  pool_name        = each.key
  depends_on       = [module.key_management_service]
}

module "key_management_service" {
  source = "../../modules/key_management_service"

  environment           = var.environment
  project_id            = var.project_id
  service_account_email = module.keygenerationservice.key_generation_service_account
  location_new_key_ring = var.location_new_key_ring
  key_sets              = local.key_sets
}

module "public_key_service" {
  source = "../public_key_service"

  environment    = var.environment
  project_id     = var.project_id
  regions        = local.public_key_service_regions
  service_domain = local.public_key_domain

  source_container_image_url = var.public_key_service_container_image_url

  # Load Balancer
  load_balancer_protocol      = var.public_key_service_load_balancer_protocol
  load_balancer_allowed_paths = var.public_key_service_load_balancer_allowed_paths

  # Load Balancer Outlier Detection
  lb_outlier_detection_enabled                               = var.public_key_service_lb_outlier_detection_enabled
  lb_outlier_detection_consecutive_errors                    = var.public_key_service_lb_outlier_detection_consecutive_errors
  lb_outlier_detection_interval_seconds                      = var.public_key_service_lb_outlier_detection_interval_seconds
  lb_outlier_detection_base_ejection_time_seconds            = var.public_key_service_lb_outlier_detection_base_ejection_time_seconds
  lb_outlier_detection_max_ejection_percent                  = var.public_key_service_lb_outlier_detection_max_ejection_percent
  lb_outlier_detection_enforcing_consecutive_errors          = var.public_key_service_lb_outlier_detection_enforcing_consecutive_errors
  lb_outlier_detection_consecutive_gateway_failure           = var.public_key_service_lb_outlier_detection_consecutive_gateway_failure
  lb_outlier_detection_enforcing_consecutive_gateway_failure = var.public_key_service_lb_outlier_detection_enforcing_consecutive_gateway_failure

  # Cloud Armor
  cloud_armor_enabled                    = var.public_key_service_cloud_armor_enabled
  cloud_armor_enable_adaptive_protection = var.public_key_service_cloud_armor_enable_adaptive_protection
  cloud_armor_preview_mode               = var.public_key_service_cloud_armor_preview_mode

  cloud_armor_rate_limit_count                       = var.public_key_service_cloud_armor_rate_limit_count
  cloud_armor_rate_limit_interval_sec                = var.public_key_service_cloud_armor_rate_limit_interval_sec
  cloud_armor_log_level                              = var.public_key_service_cloud_armor_log_level
  cloud_armor_emergency_allowlist_ips                = var.public_key_service_cloud_armor_emergency_allowlist_ips
  cloud_armor_high_block_ratio_threshold             = var.public_key_service_cloud_armor_high_block_ratio_threshold
  cloud_armor_high_block_ratio_min_samples           = var.public_key_service_cloud_armor_high_block_ratio_min_samples
  cloud_armor_rate_limit_denials_alert_threshold     = var.public_key_service_cloud_armor_rate_limit_denials_alert_threshold
  cloud_armor_rate_limit_alert_eval_period_sec       = var.public_key_service_cloud_armor_rate_limit_alert_eval_period_sec
  cloud_armor_high_block_ratio_alert_eval_period_sec = var.public_key_service_cloud_armor_high_block_ratio_alert_eval_period_sec

  # Cloud Run settings
  cpu_count             = var.public_key_service_cloud_run_cpu_count
  concurrency           = var.public_key_service_max_cloud_run_concurrency
  memory_mb             = var.public_key_service_memory_mb
  min_instance_count    = var.public_key_service_min_instances
  max_instance_count    = var.public_key_service_max_instances
  execution_environment = var.public_key_service_execution_environment

  # Spanner
  spanner_database_name = module.keydb.keydb_name
  spanner_instance_name = module.keydb.keydb_instance_name

  # Load Balancer
  managed_domain                  = local.public_key_domain
  enable_cdn                      = var.enable_public_key_service_cdn
  cdn_default_ttl_seconds         = var.public_key_service_cdn_default_ttl_seconds
  cdn_max_ttl_seconds             = var.public_key_service_cdn_max_ttl_seconds
  cdn_serve_while_stale_seconds   = var.public_key_service_cdn_serve_while_stale_seconds
  cdn_bypass_cache_header_enabled = var.public_key_service_cdn_bypass_cache_header_enabled

  # Alert
  alarms_enabled                          = var.alarms_enabled
  alarm_eval_period_sec                   = var.public_key_service_alarm_eval_period_sec
  alarm_duration_sec                      = var.public_key_service_alarm_duration_sec
  alarm_short_duration_sec                = var.public_key_service_alarm_short_duration_sec
  alert_severity_overrides                = var.alert_severity_overrides
  empty_key_set_error_threshold           = var.public_key_service_empty_key_set_error_threshold
  general_error_threshold                 = var.public_key_service_general_error_threshold
  load_balancer_alert_5xx_error_ratio     = var.public_key_service_lb_alert_5xx_error_ratio
  load_balancer_max_95_percent_latency_ms = var.public_key_service_lb_max_95_percent_latency_ms
  load_balancer_max_99_percent_latency_ms = var.public_key_service_lb_max_99_percent_latency_ms

  cloud_run_5xx_threshold                             = var.public_key_service_5xx_threshold
  cloud_run_max_execution_time_max                    = var.public_key_service_max_execution_time_max
  cloud_run_alert_on_memory_usage_important_threshold = var.public_key_service_cloud_run_memory_usage_important_threshold
  cloud_run_alert_on_memory_usage_urgent_threshold    = var.public_key_service_cloud_run_memory_usage_urgent_threshold
  cloud_run_alert_on_cpu_usage_important_threshold    = var.public_key_service_cloud_run_cpu_usage_important_threshold
  cloud_run_alert_on_cpu_usage_urgent_threshold       = var.public_key_service_cloud_run_cpu_usage_urgent_threshold
}

module "private_key_service" {
  source = "../private_key_service"


  environment    = var.environment
  project_id     = var.project_id
  regions        = local.private_key_service_regions
  service_domain = local.private_key_domain

  # Load Balancer
  load_balancer_protocol      = var.private_key_service_load_balancer_protocol
  load_balancer_allowed_paths = var.private_key_service_load_balancer_allowed_paths

  # Load Balancer Outlier Detection
  lb_outlier_detection_enabled                               = var.private_key_service_lb_outlier_detection_enabled
  lb_outlier_detection_consecutive_errors                    = var.private_key_service_lb_outlier_detection_consecutive_errors
  lb_outlier_detection_interval_seconds                      = var.private_key_service_lb_outlier_detection_interval_seconds
  lb_outlier_detection_base_ejection_time_seconds            = var.private_key_service_lb_outlier_detection_base_ejection_time_seconds
  lb_outlier_detection_max_ejection_percent                  = var.private_key_service_lb_outlier_detection_max_ejection_percent
  lb_outlier_detection_enforcing_consecutive_errors          = var.private_key_service_lb_outlier_detection_enforcing_consecutive_errors
  lb_outlier_detection_consecutive_gateway_failure           = var.private_key_service_lb_outlier_detection_consecutive_gateway_failure
  lb_outlier_detection_enforcing_consecutive_gateway_failure = var.private_key_service_lb_outlier_detection_enforcing_consecutive_gateway_failure

  allowed_invoker_service_account_emails = local.all_service_accounts
  allowed_operator_user_group            = var.allowed_operator_user_group
  source_container_image_url             = var.private_key_service_container_image_url

  # Cloud Run settings
  cpu_count                 = var.private_key_service_cloud_run_cpu_count
  memory_mb                 = var.private_key_service_cloud_run_memory_mb
  concurrency               = var.private_key_service_cloud_run_concurrency
  min_instance_count        = var.private_key_service_cloud_run_min_instances
  max_instance_count        = var.private_key_service_cloud_run_max_instances
  timeout_sec               = var.private_key_service_cloud_run_timeout_sec
  execution_environment     = var.private_key_service_execution_environment
  enable_revision_pinning   = var.private_key_service_enable_revision_pinning
  canary_region             = var.primary_region
  stable_revisions          = var.private_key_service_stable_revisions
  canary_revision           = var.private_key_service_canary_revision
  canary_traffic_percentage = var.private_key_service_canary_traffic_percentage

  # Spanner configs
  spanner_database_name      = module.keydb.keydb_name
  spanner_instance_name      = module.keydb.keydb_instance_name
  spanner_staleness_read_sec = var.spanner_staleness_read_sec

  # Vending parameters
  enable_cache             = var.enable_private_key_service_cache
  cache_refresh_in_minutes = var.private_key_service_cache_refresh_in_minutes
  key_sets_vending_config  = var.key_sets_vending_config
  key_sets_config          = var.key_sets_config

  # Alert settings
  alarms_enabled           = var.alarms_enabled
  alarm_eval_period_sec    = var.private_key_service_alarm_eval_period_sec
  alarm_duration_sec       = var.private_key_service_alarm_duration_sec
  alert_severity_overrides = var.alert_severity_overrides

  get_encrypted_private_key_general_error_threshold = var.get_encrypted_private_key_general_error_threshold
  exception_alert_threshold                         = var.private_key_service_exception_alert_threshold
  config_read_alert_threshold                       = var.private_key_service_config_read_alert_threshold

  cloud_run_5xx_threshold                             = var.private_key_service_cloud_run_5xx_threshold
  cloud_run_alert_on_memory_usage_important_threshold = var.private_key_service_cloud_run_memory_usage_important_threshold
  cloud_run_alert_on_memory_usage_urgent_threshold    = var.private_key_service_cloud_run_memory_usage_urgent_threshold
  cloud_run_alert_on_cpu_usage_important_threshold    = var.private_key_service_cloud_run_cpu_usage_important_threshold
  cloud_run_alert_on_cpu_usage_urgent_threshold       = var.private_key_service_cloud_run_cpu_usage_urgent_threshold
  cloud_run_max_execution_time_max                    = var.private_key_service_cloud_run_max_execution_time_max

  load_balancer_alert_5xx_error_ratio     = var.private_key_service_lb_alert_5xx_error_ratio
  load_balancer_max_95_percent_latency_ms = var.private_key_service_lb_max_95_percent_latency_ms
  load_balancer_max_99_percent_latency_ms = var.private_key_service_lb_max_99_percent_latency_ms
}

module "private_key_service_addon" {
  count  = var.private_key_service_addon_container_image_url == "" ? 0 : 1
  source = "../private_key_service"

  environment = local.private_key_service_addtional_environment
  project_id  = var.project_id
  regions     = local.private_key_service_regions

  service_domain     = local.private_key_domain_additional
  display_identifier = "Pre${var.environment}"

  # Load Balancer
  load_balancer_protocol      = var.private_key_service_load_balancer_protocol
  load_balancer_allowed_paths = var.private_key_service_load_balancer_allowed_paths

  # Load Balancer Outlier Detection
  lb_outlier_detection_enabled                               = var.private_key_service_lb_outlier_detection_enabled
  lb_outlier_detection_consecutive_errors                    = var.private_key_service_lb_outlier_detection_consecutive_errors
  lb_outlier_detection_interval_seconds                      = var.private_key_service_lb_outlier_detection_interval_seconds
  lb_outlier_detection_base_ejection_time_seconds            = var.private_key_service_lb_outlier_detection_base_ejection_time_seconds
  lb_outlier_detection_max_ejection_percent                  = var.private_key_service_lb_outlier_detection_max_ejection_percent
  lb_outlier_detection_enforcing_consecutive_errors          = var.private_key_service_lb_outlier_detection_enforcing_consecutive_errors
  lb_outlier_detection_consecutive_gateway_failure           = var.private_key_service_lb_outlier_detection_consecutive_gateway_failure
  lb_outlier_detection_enforcing_consecutive_gateway_failure = var.private_key_service_lb_outlier_detection_enforcing_consecutive_gateway_failure

  allowed_invoker_service_account_emails = local.all_service_accounts
  allowed_operator_user_group            = var.allowed_operator_user_group
  source_container_image_url             = var.private_key_service_addon_container_image_url

  # Cloud Run settings
  cpu_count             = var.private_key_service_cloud_run_cpu_count
  memory_mb             = var.private_key_service_cloud_run_memory_mb
  concurrency           = var.private_key_service_cloud_run_concurrency
  min_instance_count    = var.private_key_service_cloud_run_min_instances
  max_instance_count    = var.private_key_service_cloud_run_max_instances
  timeout_sec           = var.private_key_service_cloud_run_timeout_sec
  execution_environment = var.private_key_service_execution_environment

  # private_key_service_addon does not use canary deployments
  enable_revision_pinning   = false
  canary_region             = var.primary_region
  stable_revisions          = {}
  canary_revision           = null
  canary_traffic_percentage = 0

  # Spanner configs
  spanner_database_name      = module.keydb.keydb_name
  spanner_instance_name      = module.keydb.keydb_instance_name
  spanner_staleness_read_sec = var.spanner_staleness_read_sec

  # Vending parameters
  enable_cache             = var.enable_private_key_service_cache
  cache_refresh_in_minutes = var.private_key_service_cache_refresh_in_minutes
  key_sets_vending_config  = var.key_sets_vending_config
  key_sets_config          = var.key_sets_config

  # Alert settings
  alarms_enabled           = var.alarms_enabled
  alarm_eval_period_sec    = var.private_key_service_alarm_eval_period_sec
  alarm_duration_sec       = var.private_key_service_alarm_duration_sec
  alert_severity_overrides = var.private_key_service_addon_alert_severity_overrides

  get_encrypted_private_key_general_error_threshold = var.get_encrypted_private_key_general_error_threshold
  exception_alert_threshold                         = var.private_key_service_exception_alert_threshold
  config_read_alert_threshold                       = var.private_key_service_config_read_alert_threshold

  cloud_run_5xx_threshold                             = var.private_key_service_cloud_run_5xx_threshold
  cloud_run_alert_on_memory_usage_important_threshold = var.private_key_service_cloud_run_memory_usage_important_threshold
  cloud_run_alert_on_memory_usage_urgent_threshold    = var.private_key_service_cloud_run_memory_usage_urgent_threshold
  cloud_run_alert_on_cpu_usage_important_threshold    = var.private_key_service_cloud_run_cpu_usage_important_threshold
  cloud_run_alert_on_cpu_usage_urgent_threshold       = var.private_key_service_cloud_run_cpu_usage_urgent_threshold
  cloud_run_max_execution_time_max                    = var.private_key_service_cloud_run_max_execution_time_max

  load_balancer_alert_5xx_error_ratio     = var.private_key_service_lb_alert_5xx_error_ratio
  load_balancer_max_95_percent_latency_ms = var.private_key_service_lb_max_95_percent_latency_ms
  load_balancer_max_99_percent_latency_ms = var.private_key_service_lb_max_99_percent_latency_ms
}

module "key_migration_tool" {
  source = "../key_migration_tool"
  count  = var.run_key_migration_tool ? 1 : 0

  environment = var.environment
  project_id  = var.project_id
  region      = var.primary_region

  source_container_image_url = var.key_migration_tool_container_image_url

  # Cloud Run Job settings
  cpu_count            = var.key_migration_tool_cpu_count
  memory_mb            = var.key_migration_tool_memory_mb
  max_retries          = var.key_migration_tool_max_retries
  task_timeout_seconds = var.key_migration_tool_task_timeout_seconds

  # Spanner and access configs
  spanner_database_name = module.keydb.keydb_name
  spanner_instance_name = module.keydb.keydb_instance_name

  # Key Rings
  legacy_migration_key_ring_id = module.key_management_service.kms_key_ring_id
  migration_key_ring_id        = module.key_management_service.kms_key_ring_id

  # Environment variables
  migration_kek_base_uri      = local.migration_kms_key_base_uri
  migration_peer_kek_base_uri = local.migration_peer_coordinator_kms_key_base_uri
  migration_key_sets          = var.key_migration_tool_key_sets
  migrator_mode               = var.key_migration_tool_migrator_mode
  dry_run                     = var.key_migration_tool_dry_run
}

module "domain_a_records" {
  source = "../../modules/domain_a_records"

  primary_region      = var.primary_region
  primary_region_zone = var.primary_region_zone

  parent_domain_name         = var.parent_domain_name
  parent_domain_name_project = var.parent_domain_name_project

  service_domain_to_address_map = local.service_domain_to_address_map
}

# parameters

module "keydb_instance_id" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "SPANNER_INSTANCE"
  parameter_value = module.keydb.keydb_instance_name
}

module "keydb_name" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "KEY_DB_NAME"
  parameter_value = module.keydb.keydb_name
}

module "pubsub_id" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "SUBSCRIPTION_ID"
  parameter_value = module.keygenerationservice.subscription_id
}

module "key_generation_project_id" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "PROJECT_ID"
  parameter_value = var.project_id
}

module "key_generation_count" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "NUMBER_OF_KEYS_TO_CREATE"
  parameter_value = var.key_generation_count
}

module "key_generation_validity_in_days" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "KEYS_VALIDITY_IN_DAYS"
  parameter_value = var.key_generation_validity_in_days
}

module "key_generation_ttl_in_days" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "KEY_TTL_IN_DAYS"
  parameter_value = var.key_generation_ttl_in_days
}

module "key_generation_max_days_ahead" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "CREATE_MAX_DAYS_AHEAD"
  parameter_value = var.key_generation_max_days_ahead
}

module "key_storage_service_base_url" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "KEY_STORAGE_SERVICE_BASE_URL"
  parameter_value = var.key_storage_service_base_url
}

module "peer_coordinator_wip_provider" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_WIP_PROVIDER"
  parameter_value = var.peer_coordinator_wip_provider
}

module "peer_coordinator_service_account" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_SERVICE_ACCOUNT"
  parameter_value = var.peer_coordinator_service_account
}

module "key_id_type" {
  count           = var.key_id_type == "" ? 0 : 1
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "KEY_ID_TYPE"
  parameter_value = var.key_id_type
}

module "key_sets_config" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "KEY_SETS_CONFIG"
  parameter_value = jsonencode(var.key_sets_config)
}

module "key_sets_parameter_config" {
  source         = "../../modules/parameter_manager"
  project        = var.project_id
  environment    = var.environment
  parameter_name = "KEY_SETS_CONFIG"
  parameter_data = jsonencode(var.key_sets_config)
}

module "kms_key_ring_uri" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "KMS_KEY_BASE_URI"
  parameter_value = local.kms_key_base_uri
}

module "peer_coordinator_kms_key_ring_uri" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_KMS_KEY_BASE_URI"
  parameter_value = var.peer_coordinator_kms_key_base_uri
}

module "migration_kms_key_ring_uri" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "MIGRATION_KMS_KEY_BASE_URI"
  parameter_value = local.migration_kms_key_base_uri
}

module "migration_peer_coordinator_kms_key_ring_uri" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "MIGRATION_PEER_COORDINATOR_KMS_KEY_BASE_URI"
  parameter_value = local.migration_peer_coordinator_kms_key_base_uri
}

module "populate_migration_key_data" {
  source          = "../../modules/secret_manager"
  environment     = var.environment
  parameter_name  = "POPULATE_MIGRATION_KEY_DATA"
  parameter_value = var.populate_migration_key_data
}
