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

################################################################################
# Global Variables.
################################################################################

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "allowed_operator_user_group" {
  description = "Google group of allowed operators to which to give API access."
  type        = string
  default     = null
}

variable "primary_region" {
  description = "Region where all services will be created."
  type        = string
}

variable "primary_region_zone" {
  description = "Region zone where all services will be created."
  type        = string
}

variable "secondary_region" {
  description = "Region where all services will be replicated."
  type        = string
}

variable "secondary_region_zone" {
  description = "Region zone where all services will be replicated."
  type        = string
}

################################################################################
# Global Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for mpkhs services."
  type        = bool
}

variable "quota_alert_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = number
}

variable "quota_alert_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation of quota usage. Example: '60'."
  type        = number
}

variable "quota_alert_max_over_minutes" {
  description = "Number of minutes over which to get a max quota usage."
  type        = number
}

variable "quota_alert_threshold_important" {
  description = "Percentage of quota usage that triggers an important alert. Example: 0.6"
  type        = number
}

variable "quota_alert_threshold_urgent" {
  description = "Percentage of quota usage that triggers an urgent alert. Example: 0.9"
  type        = number
}

################################################################################
# Spanner Variables.
################################################################################

variable "spanner_instance_config" {
  description = "Config value for the Spanner Instance. Example: 'nam10'."
  type        = string
}

variable "spanner_processing_units" {
  description = "Spanner's compute capacity. 1000 processing units = 1 node and must be set as a multiple of 100."
  type        = number
}

variable "spanner_staleness_read_sec" {
  description = "Acceptable read staleness in seconds."
  type        = string
}

variable "spanner_custom_configuration_name" {
  description = "Name for the custom spanner configuration to be created."
  type        = string
  nullable    = true
}

variable "spanner_custom_configuration_display_name" {
  description = "Display name for the custom spanner configuration to be created."
  type        = string
}

variable "spanner_custom_configuration_base_config" {
  description = "Base spanner configuration used as starting basis for custom configuration."
  type        = string
}

variable "spanner_custom_configuration_read_replica_location" {
  description = "Region used in custom configuration as an additional read replica."
  type        = string
}

################################################################################
# Key Generation Variables.
################################################################################

variable "key_generation_region" {
  description = "Region to use for the key generation MIG."
  type        = string
  nullable    = false
}

variable "key_generation_allow_stopping_for_update" {
  description = "If true, allows Terraform to stop the key generation instances to update their properties. If you try to update a property that requires stopping the instances without setting this field, the update will fail."
  type        = bool
  default     = false
}

variable "key_generation_service_container_image_url" {
  description = "The Key Generation Application docker image."
  type        = string
}

variable "key_generation_count" {
  description = "Number of keys to generate at a time."
  type        = number

  validation {
    condition     = var.key_generation_count > 0
    error_message = "Must be greater than 0."
  }
}

variable "key_generation_validity_in_days" {
  description = "Number of days keys will be valid. Should be greater than generation days for failover validity."
  type        = number

  validation {
    condition     = var.key_generation_validity_in_days > 0
    error_message = "Must be greater than 0."
  }
}

variable "key_generation_ttl_in_days" {
  description = "Keys will be deleted from the database this number of days after creation time."
  type        = number
  validation {
    condition     = var.key_generation_ttl_in_days > 0
    error_message = "Must be greater than 0."
  }
}

variable "key_generation_max_days_ahead" {
  description = "Max number of days ahead that a key can be created."
  type        = number
  validation {
    condition     = var.key_generation_max_days_ahead > 1
    error_message = "Must be greater than 1."
  }
}

variable "key_generation_cron_schedule" {
  description = <<-EOT
    Frequency for key generation cron job. Must be valid cron statement. Default is every Monday at 10AM
    See documentation for more details: https://cloud.google.com/scheduler/docs/configuring/cron-job-schedules
  EOT
  type        = string
}

variable "key_generation_cron_time_zone" {
  description = "Time zone to be used with cron schedule."
  type        = string
}

variable "instance_disk_image" {
  description = "The image from which to initialize the key generation instance disk for TEE."
  type        = string
}

variable "key_generation_logging_enabled" {
  description = "Whether to enable logging for Key Generation instance."
  type        = bool
}

variable "key_generation_monitoring_enabled" {
  description = "Whether to enable monitoring for Key Generation instance."
  type        = bool
}

variable "key_generation_tee_restart_policy" {
  description = "The TEE restart policy. Currently only supports Never."
  type        = string
}

variable "key_generation_tee_allowed_sa" {
  description = "The service account provided by Coordinator B for key generation instance to impersonate."
  type        = string
}

variable "key_generation_undelivered_messages_threshold" {
  description = "Total Queue Messages greater than this to send alert."
  type        = number
}

variable "key_generation_error_threshold" {
  description = "Total key generation errors greater than this to send alert."
  type        = number
}

variable "key_generation_alignment_period" {
  description = "Alignment period of key generation alert metrics in seconds. This value should match the period of the cron schedule."
  type        = number

  validation {
    # This value should match the period of the cron schedule.
    # Used for the alignment period of alert metrics.
    # Max 81,000 seconds due to the max GCP metric-threshold evaluation period
    # (23 hours, 30 minutes) plus an extra hour to allow fluctuations in
    # execution time.
    condition     = var.key_generation_alignment_period > 0 && var.key_generation_alignment_period < 81000
    error_message = "Must be between 0 and 81,000 seconds."
  }
}

variable "key_generation_single_keyset_alignment_periods" {
  description = "Total number of alignment periods a keyset needs to fail before alerting."
  type        = number
}

variable "key_generation_create_alert_alignment_periods" {
  description = "Total number of key generation alignment periods that need to pass without a create to alert."
  type        = number
}

variable "key_storage_service_base_url" {
  description = "Base url for key storage service for peer coordinator."
  type        = string
}

variable "peer_coordinator_wip_provider" {
  description = "Peer coordinator wip provider address."
  type        = string
}

variable "peer_coordinator_service_account" {
  description = "Service account generated from peer coordinator."
  type        = string
}

variable "key_id_type" {
  description = "Key ID Type"
  type        = string
}

variable "allowed_operators" {
  description = <<EOT
  Authorized decrypters that have attested access to perform decryption.

  Attributes:
    key                        (string) - The name of the decrypters set.
    value.service_accounts     (string) - The list of service account emails of the decrypters.
    value.image_references     (string) - The list of allowed image references.
    value.image_digests        (string) - The list of allowed image digests.
    value.image_signature_key_ids (string) - The list of required signature algorithm key ids.
  EOT
  type = map(object({
    service_accounts        = list(string)
    key_sets                = list(string)
    image_references        = list(string)
    image_digests           = list(string)
    image_signature_key_ids = list(string)
  }))
  default = {}
}

variable "location_new_key_ring" {
  description = "Location for the global key ring."
  type        = string
  nullable    = true
}

variable "key_generation_precheck_error_threshold" {
  description = "Total key generation precheck errors greater than this to send alert."
  type        = number
}

variable "key_generation_precheck_alignment_period" {
  description = "Alignment period of key generation precheck alert metrics in seconds."
  type        = number

  validation {
    # Used for the alignment period of alert metrics.
    # Max 81,000 seconds due to the max GCP metric-threshold evaluation period
    # (23 hours, 30 minutes) plus an extra hour to allow fluctuations in
    # execution time.
    condition     = var.key_generation_precheck_alignment_period > 0 && var.key_generation_precheck_alignment_period < 81000
    error_message = "Must be greater than 0 and less than 81,000 seconds."
  }
}

################################################################################
# Routing Variables.
################################################################################

variable "parent_domain_name" {
  description = "Custom domain name to use with key hosting APIs."
  type        = string
}

variable "parent_domain_name_project" {
  description = "Project ID where custom domain name hosted zone is located."
  type        = string
}

variable "service_subdomain_suffix" {
  description = "When set, the value replaces `-$${var.environment}` as the service subdomain suffix."
  type        = string
}

variable "public_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the Public Key Service."
  type        = string
}

### Public Key Service

variable "public_key_service_container_image_url" {
  description = "The full path (registry + tag) to the container image used to deploy Public Key Service."
  type        = string
  nullable    = false
}

################################################################################
# Public Key Service Load Balancer Variables.
################################################################################

variable "public_key_service_load_balancer_allowed_paths" {
  description = "List of allowed paths for the public key service load balancer. Requests to other paths will be denied."
  type        = list(string)
}

variable "public_key_service_load_balancer_protocol" {
  description = "The protocol the load balancer uses to communicate with backends."
  type        = string
}

################################################################################
# Public Key Service Cloud Run Variables.
################################################################################

variable "public_key_service_cr_regions" {
  description = "Additional regions beyond primary and secondary that Public KS will run in."
  type        = list(string)
  nullable    = false
}

variable "public_key_service_cloud_run_cpu_count" {
  description = "Number of cpus to allocate for Public Key Service cloud run instance."
  type        = number
}

variable "public_key_service_max_cloud_run_concurrency" {
  description = "Maximum request concurrency for Public Key Service cloud run instance."
  type        = number
}

variable "public_key_service_memory_mb" {
  description = "Memory size in MB for public key service."
  type        = number
}

variable "public_key_service_min_instances" {
  description = "The minimum number of instances that may coexist at a given time."
  type        = number
}

variable "public_key_service_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
}

variable "public_key_service_execution_environment" {
  description = "The sandbox environment to host Public KS."
  type        = string
  nullable    = false
}

### Private Key Service

variable "private_key_service_additional_regions" {
  description = "Additional regions beyond primary and secondary that Private KS will run in."
  type        = list(string)
  nullable    = false
}

variable "private_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the private key service."
  type        = string
}

variable "private_key_service_container_image_url" {
  description = "The full path (registry + tag) to the container image used to deploy Private Key Service."
  type        = string
  nullable    = false
}

variable "private_key_service_addon_container_image_url" {
  description = "The full path (registry + tag) to the container image used to deploy an additional Private Key Service."
  type        = string
  nullable    = false
}

variable "private_key_service_addon_alert_severity_overrides" {
  description = "Alerts severity overrides for the addon Private Key Service."
  type        = map(string)
  default     = {}
}

variable "private_key_service_cloud_run_cpu_count" {
  description = "Number of cpus to allocate for private key service cloud run instance."
  type        = number
}

variable "private_key_service_cloud_run_concurrency" {
  description = "Request concurrency for private key service cloud run instance."
  type        = number
}

variable "enable_private_key_service_cache" {
  description = "Variable to enable server side cache in the Private Key Service."
  type        = bool
}

variable "private_key_service_cache_refresh_in_minutes" {
  description = "Frequency the private key service caches is refreshed."
  type        = number
}


################################################################################
# Private Key Service Load Balancer Variables.
################################################################################

variable "private_key_service_load_balancer_protocol" {
  description = "The protocol the load balancer uses to communicate with backends."
  type        = string
}

variable "private_key_service_load_balancer_allowed_paths" {
  description = "List of allowed paths for the private key service load balancer. Requests to other paths will be denied."
  type        = list(string)
}

################################################################################
# Private Key Service Cloud Run Variables.
################################################################################

variable "private_key_service_cloud_run_memory_mb" {
  description = "Memory size in MB for private key service cloud run."
  type        = number
}

variable "private_key_service_cloud_run_min_instances" {
  description = "The minimum number of cloud run instances that may coexist at a given time."
  type        = number
}

variable "private_key_service_cloud_run_max_instances" {
  description = "The maximum number of cloud run instances that may coexist at a given time."
  type        = number
}

variable "private_key_service_cloud_run_timeout_sec" {
  description = "Max allowed time for an Private KS instance to respond to a request."
  type        = number
}

variable "private_key_service_execution_environment" {
  description = "The sandbox environment to host Private KS."
  type        = string
  nullable    = false
}

################################################################################
# Private Key Service CR Canary Variables.
################################################################################

variable "private_key_service_enable_revision_pinning" {
  description = "Enables the canary feature. When enabled valid stable_revisions must be provided."
  nullable    = false
  type        = bool
}

variable "private_key_service_stable_revisions" {
  description = "Map of regions to their stable revision name used for version pinning."
  type        = map(string)
}

variable "private_key_service_canary_revision" {
  description = "The revision name for the canary service used for version pinning."
  type        = string
}

variable "private_key_service_canary_traffic_percentage" {
  description = "The percentage of traffic to route to the canary service."
  nullable    = false
  type        = number
}

################################################################################
# Key Management Variables.
################################################################################

variable "enable_public_key_service_cdn" {
  description = "Enable Get Public Key API CDN."
  type        = bool
}

variable "public_key_service_cdn_default_ttl_seconds" {
  description = "Default CDN TTL seconds to use when no cache headers are present."
  type        = number
}

variable "public_key_service_cdn_max_ttl_seconds" {
  description = "Maximum CDN TTL seconds that cache header directive cannot surpass."
  type        = number
}

variable "public_key_service_cdn_serve_while_stale_seconds" {
  description = "Maximum CDN TTL seconds that cache header directive cannot surpass."
  type        = number
}

variable "public_key_service_cdn_bypass_cache_header_enabled" {
  description = "Enable bypass cache header for the public key service CDN."
  type        = bool
}

################################################################################
# Public Key Alarm Variables.
################################################################################

variable "public_key_service_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = number
}

variable "public_key_service_alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = number
}

variable "public_key_service_alarm_short_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met (for short-span alerts). Example: '60'."
  type        = number
}

variable "public_key_service_max_execution_time_max" {
  description = "Max execution time in ms to send alarm. Example: 9999."
  type        = number
}

variable "public_key_service_5xx_threshold" {
  description = "Cloud Function 5xx error count greater than this to send alarm. Example: 0."
  type        = number
}

variable "public_key_service_cloud_run_memory_usage_important_threshold" {
  description = "Memory usage of the Cloud Run should be higher than this value to create bug."
  type        = number
}

variable "public_key_service_cloud_run_memory_usage_urgent_threshold" {
  description = "Memory usage of the Cloud Run should be higher than this value to page."
  type        = number
}

variable "public_key_service_cloud_run_cpu_usage_important_threshold" {
  description = "CPU usage of the Cloud Run should be higher than this value to create bug."
  type        = number
}

variable "public_key_service_cloud_run_cpu_usage_urgent_threshold" {
  description = "CPU usage of the Cloud Run should be higher than this value to page."
  type        = number
}

variable "public_key_service_lb_alert_5xx_error_ratio" {
  description = "Private KS Load Balancer 5xx error ratio greater than this to send alarm. Example: 0.1"
  type        = number
}

variable "public_key_service_lb_max_95_percent_latency_ms" {
  description = "Public Key Service Load Balancer max 95% latency to send alert. Measured in milliseconds. Example: 5000."
  type        = number
}

variable "public_key_service_lb_max_99_percent_latency_ms" {
  description = "Public Key Service Load Balancer max 99% latency to send alert. Measured in milliseconds. Example: 5000."
  type        = number
}

variable "public_key_service_empty_key_set_error_threshold" {
  description = "Get Public Key Empty Key Set error count greater than this to send alarm. Example: 0."
  type        = number
}

variable "public_key_service_general_error_threshold" {
  description = "Get Public Key General error count greater than this to send alarm. Example: 0."
  type        = number
}

################################################################################
# Public Key Service Load Balancer Outlier Detection Variables.
################################################################################

variable "public_key_service_lb_outlier_detection_enabled" {
  description = "Enable outlier detection for the public key service load balancer."
  type        = bool
}

variable "public_key_service_lb_outlier_detection_consecutive_errors" {
  description = "Number of consecutive errors before a backend is ejected for the public key service load balancer."
  type        = number
}

variable "public_key_service_lb_outlier_detection_interval_seconds" {
  description = "The interval time in seconds for outlier detection for the public key service load balancer."
  type        = number
}

variable "public_key_service_lb_outlier_detection_base_ejection_time_seconds" {
  description = "The base ejection time in seconds for the public key service load balancer."
  type        = number
}

variable "public_key_service_lb_outlier_detection_max_ejection_percent" {
  description = "The maximum percentage of backends that can be ejected for the public key service load balancer."
  type        = number
}

variable "public_key_service_lb_outlier_detection_enforcing_consecutive_errors" {
  description = "The percentage of backends that must have consecutive errors before outlier detection is enforced for the public key service load balancer."
  type        = number
}

variable "public_key_service_lb_outlier_detection_consecutive_gateway_failure" {
  description = "The number of consecutive gateway failures before a backend is ejected for the public key service load balancer."
  type        = number
}

variable "public_key_service_lb_outlier_detection_enforcing_consecutive_gateway_failure" {
  description = "The percentage of backends that must have consecutive gateway failures before outlier detection is enforced for the public key service load balancer."
  type        = number
}

################################################################################
# Cloud Armor Security Policy Variables for Public Key Service.
################################################################################

variable "public_key_service_cloud_armor_enabled" {
  description = "If true, creates and attaches the Cloud Armor security policy for the public key service."
  type        = bool
}

variable "public_key_service_cloud_armor_enable_adaptive_protection" {
  description = "Whether to enable Cloud Armor Adaptive Protection for Public Key Service."
  type        = bool
}

variable "public_key_service_cloud_armor_preview_mode" {
  description = "If true, the Cloud Armor security policy for the public key service is in preview mode."
  type        = bool
}

variable "public_key_service_cloud_armor_rate_limit_count" {
  description = "Rate limit count for the Public Key Service Cloud Armor policy."
  type        = number
}

variable "public_key_service_cloud_armor_rate_limit_interval_sec" {
  description = "Rate limit interval in seconds for the Public Key Service Cloud Armor policy."
  type        = number
}

variable "public_key_service_cloud_armor_log_level" {
  description = "Log level for the Public Key Service Cloud Armor policy."
  type        = string
}

variable "public_key_service_cloud_armor_emergency_allowlist_ips" {
  description = "List of Emergency IPs to be allowlisted to bypass all security mechanics for Public Key Service."
  type        = list(string)
}

variable "public_key_service_cloud_armor_high_block_ratio_threshold" {
  description = "The threshold for the Public Key Service Cloud Armor high block ratio alert."
  type        = number
}

variable "public_key_service_cloud_armor_high_block_ratio_min_samples" {
  description = "Minimum total requests in the window to trigger the Public Key Service Cloud Armor high block ratio alert."
  type        = number
}

variable "public_key_service_cloud_armor_rate_limit_denials_alert_threshold" {
  description = "The threshold for the Public Key Service Cloud Armor rate limit denials alert."
  type        = number
}

variable "public_key_service_cloud_armor_rate_limit_alert_eval_period_sec" {
  description = "Amount of time (in seconds) for Public Key Service Cloud Armor rate limit alert evaluation. Example: '60'."
  type        = number
}

variable "public_key_service_cloud_armor_high_block_ratio_alert_eval_period_sec" {
  description = "Amount of time (in seconds) for Public Key Service Cloud Armor high block ratio alert evaluation. Example: '60'."
  type        = number
}

################################################################################
# Private Key Service Alarm Variables.
################################################################################

variable "private_key_service_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = number
}

variable "private_key_service_cloud_run_max_execution_time_max" {
  description = "Max execution time in ms to send alarm. Example: 9999."
  type        = number
}

variable "private_key_service_cloud_run_5xx_threshold" {
  description = "Cloud Function 5xx error count greater than this to send alarm. Example: 0."
  type        = number
}

variable "private_key_service_cloud_run_memory_usage_important_threshold" {
  description = "Memory usage of the Cloud Run should be higher than this value to create bug."
  type        = number
}

variable "private_key_service_cloud_run_memory_usage_urgent_threshold" {
  description = "Memory usage of the Cloud Run should be higher than this value to page."
  type        = number
}

variable "private_key_service_cloud_run_cpu_usage_important_threshold" {
  description = "CPU usage of the Cloud Run should be higher than this value to create bug."
  type        = number
}

variable "private_key_service_cloud_run_cpu_usage_urgent_threshold" {
  description = "CPU usage of the Cloud Run should be higher than this value to page."
  type        = number
}

variable "private_key_service_lb_alert_5xx_error_ratio" {
  description = "Private KS Load Balancer 5xx error ratio greater than this to send alarm. Example: 0.1"
  type        = number
}

variable "private_key_service_lb_max_95_percent_latency_ms" {
  description = "Private Key Service Load Balancer max 95% latency to send alert. Measured in milliseconds. Example: 5000."
  type        = number
}

variable "private_key_service_lb_max_99_percent_latency_ms" {
  description = "Private Key Service Load Balancer max 99% latency to send alert. Measured in milliseconds. Example: 5000."
  type        = number
}

variable "private_key_service_alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = number
}

variable "key_sets_config" {
  description = <<EOT
  Configuration for key sets managed by Key Generation.

  Note: For backward compatibility, if a config is not defined. A default key set with name = "" is created.

  Attributes:
    key_sets                         (list) - The list of individual key set configuration, one for each unique key set.
    key_sets[].name                  (string) - The unique set name for the key set, the value should be a valid URL path segment (e.g. ^[a-zA-Z0-9\\-\\._~]+$).
    key_sets[].tink_template         (string) - The Tink template to be used for key generation.
    key_sets[].count                 (number) - The number of keys to be generated.
    key_sets[].validity_in_days      (number) - Number of days the generated keys will be valid. If set to 0, the keys will never expire.
    key_sets[].ttl_in_days           (number) - Number of days the generated keys will be kept in the database. If set to 0, the keys will never be deleted.
    key_sets[].create_max_days_ahead (optional(number)) - Number of days ahead that a key can be created.
    key_sets[].overlap_period_days   (optional(number)) - Number of days each consecutive active set should overlap.
    key_sets[].backfill_days         (optional(number)) - Number of days allowed for a key to be used past its expiration date for a backfill job.
  EOT
  type = object({
    key_sets = list(object({
      name                  = string
      tink_template         = string
      count                 = number
      validity_in_days      = number
      ttl_in_days           = number
      create_max_days_ahead = optional(number)
      overlap_period_days   = optional(number)
      backfill_days         = optional(number)
    }))
  })
}

variable "get_encrypted_private_key_general_error_threshold" {
  description = "Get Encrypted Key General error count greater than this to send alarm. Example: 0."
  type        = number
}

variable "private_key_service_exception_alert_threshold" {
  description = "Private KS exception count greater than this to send alarm. Example: 0."
  type        = number
}

variable "private_key_service_config_read_alert_threshold" {
  description = "Private KS config read error count greater than this to send alarm. Example: 0."
  type        = number
}

################################################################################
# Private Key Service Load Balancer Outlier Detection Variables.
################################################################################

variable "private_key_service_lb_outlier_detection_enabled" {
  description = "Enable outlier detection for the private key service load balancer."
  type        = bool
}

variable "private_key_service_lb_outlier_detection_consecutive_errors" {
  description = "Number of consecutive errors before a backend is ejected for the private key service load balancer."
  type        = number
}

variable "private_key_service_lb_outlier_detection_interval_seconds" {
  description = "The interval time in seconds for outlier detection for the private key service load balancer."
  type        = number
}

variable "private_key_service_lb_outlier_detection_base_ejection_time_seconds" {
  description = "The base ejection time in seconds for the private key service load balancer."
  type        = number
}

variable "private_key_service_lb_outlier_detection_max_ejection_percent" {
  description = "The maximum percentage of backends that can be ejected for the private key service load balancer."
  type        = number
}

variable "private_key_service_lb_outlier_detection_enforcing_consecutive_errors" {
  description = "The percentage of backends that must have consecutive errors before outlier detection is enforced for the private key service load balancer."
  type        = number
}

variable "private_key_service_lb_outlier_detection_consecutive_gateway_failure" {
  description = "The number of consecutive gateway failures before a backend is ejected for the private key service load balancer."
  type        = number
}

variable "private_key_service_lb_outlier_detection_enforcing_consecutive_gateway_failure" {
  description = "The percentage of backends that must have consecutive gateway failures before outlier detection is enforced for the private key service load balancer."
  type        = number
}

################################################################################

variable "alert_severity_overrides" {
  description = "Alerts severity overrides."
  type        = map(string)
}

variable "peer_coordinator_kms_key_base_uri" {
  description = "Kms key base url from peer coordinator."
  type        = string
}

################################################################################
# Migration Variables.
################################################################################

variable "populate_migration_key_data" {
  description = <<EOT
  Controls whether to populate the migration columns when generating keys.

  Note: This should only should only be used in preparation for or during a migration.
  EOT
  type        = string
}

variable "key_sets_vending_config" {
  description = <<EOT
  Configuration for controlling key set vending.

  Attributes:
    allowed_migrators (list(string)) - The list of individual key set and/or caller emails allowed to consume migration key data.
    cache_users       (list(string)) - The list of individual key set and/or caller emails allowed to use cache.
  EOT
  type = object({
    allowed_migrators = list(string)
    cache_users       = optional(list(string))
  })
}

variable "migration_peer_coordinator_kms_key_base_uri" {
  description = "Migration kms key base url from the peer coordinator."
  type        = string
}

variable "key_migration_tool_container_image_url" {
  description = "The full path (registry + tag) to the container image used to deploy the Key Migration Tool."
  type        = string
}

variable "key_migration_tool_cpu_count" {
  description = "How many CPUs to give to the migration tool's instance. Supported values are 1,2,4 and 8"
  type        = number
  nullable    = false
}

variable "key_migration_tool_memory_mb" {
  description = "How much memory in MB to give to the migration tool's instance. e.g. 256."
  type        = number
  nullable    = false
}

variable "key_migration_tool_max_retries" {
  description = "How many times to retry a failed task in the migration tool."
  type        = number
  nullable    = false
}

variable "key_migration_tool_task_timeout_seconds" {
  description = <<EOF
  The maximum amount of time in seconds that the migration tool job is allowed to run for each try.
  The maximum run time will be `key_migration_tool_timeout_seconds * key_migration_tool_max_retries`.
  EOF
  type        = number
  nullable    = false
}

variable "key_migration_tool_migrator_mode" {
  description = <<EOF
  The migration tool's mode:
      'generate': Creates migration key data for keys and
                  then populated those fields in the database.
      'migrate': Updates the database by overwriting the original key material.
      'cleanup': Removes the migration key data from the database migration columns.
  EOF
  type        = string
}

variable "key_migration_tool_dry_run" {
  description = "When true, no database updates will be made."
  type        = bool
  nullable    = false
}

variable "run_key_migration_tool" {
  description = "Enables the Key Migration Tool."
  type        = bool
  nullable    = false
}

variable "key_migration_tool_key_sets" {
  description = <<EOT
  Configuration for controlling what key set should be migrated.

  Attributes:
    allowed_keysets (list(string)) - The list of individual key set specified
    for the migration tool to act upon.
  EOT
  type = object({
    allowed_keysets = list(string)
  })
  nullable = false
}
