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

test {
  parallel = true
}
mock_provider "google" {
  source          = "../../../../../tools/tftesting/tfmocks/google/"
  override_during = plan
}
mock_provider "google-beta" {
  source          = "../../../../../tools/tftesting/tfmocks/google-beta/"
  override_during = plan
}
mock_provider "null" {
  source          = "../../../../../tools/tftesting/tfmocks/null/"
  override_during = plan
}

# All run blocks should have "command = plan".
# Take great care when writing tests with "command = apply".
variables {
  environment                      = "environment"
  project_id                       = ""
  network                          = ""
  subnet_id                        = ""
  region                           = "region"
  internet_tag_for_otel            = "egress-internet"
  user_provided_collector_sa_email = ""
  collector_instance_type          = ""
  collector_startup_script         = ""
  collector_service_port_name      = ""
  collector_service_port           = 0
  collector_min_instance_ready_sec = 0
  collector_regional_config = {
    "us-central1" = {
      zonal_config = {
        "us-central1-c" = {
          min_collector_count              = 1
          max_collector_count              = 2
          collector_cpu_utilization_target = 0.8
        }
      }
    },
    "us-east1" = {
      zonal_config = {
        "us-east1-c" = {
          min_collector_count              = 1
          max_collector_count              = 2
          collector_cpu_utilization_target = 0.8
        }
      }
    }
  }
  subnets_per_region = {
    "us-central1" = "subnet_id1"
    "us-east1"    = "subnet_id2"
  }
  collector_exceed_cpu_usage_alarm = {
    alignment_period_sec = 0
    auto_close_sec       = 0
    duration_sec         = 0
    enable_alarm         = false
    severity             = ""
    threshold            = 0
  }
  collector_exceed_memory_usage_alarm = {
    alignment_period_sec = 0
    auto_close_sec       = 0
    duration_sec         = 0
    enable_alarm         = false
    severity             = ""
    threshold            = 0
  }
  collector_startup_error_alarm = {
    alignment_period_sec = 0
    auto_close_sec       = 0
    duration_sec         = 0
    enable_alarm         = false
    severity             = ""
    threshold            = 0
  }
  collector_crash_error_alarm = {
    alignment_period_sec = 0
    auto_close_sec       = 0
    duration_sec         = 0
    enable_alarm         = false
    severity             = ""
    threshold            = 0
  }
  export_metric_to_collector_error_alarm = {
    alignment_period_sec = 0
    auto_close_sec       = 0
    duration_sec         = 0
    enable_alarm         = false
    severity             = ""
    threshold            = 0
  }
  collector_queue_size_alarm = {
    alignment_period_sec = 0
    auto_close_sec       = 0
    duration_sec         = 0
    enable_alarm         = false
    severity             = ""
    threshold            = 0
  }
  collector_send_metric_failure_rate_alarm = {
    alignment_period_sec = 0
    auto_close_sec       = 0
    duration_sec         = 0
    enable_alarm         = false
    severity             = ""
    threshold            = 0
  }
  collector_refuse_metric_rate_alarm = {
    alignment_period_sec = 0
    auto_close_sec       = 0
    duration_sec         = 0
    enable_alarm         = false
    severity             = ""
    threshold            = 0
  }
}

run "doesnt_create_alarms" {
  command = plan

  assert {
    condition     = length(google_monitoring_alert_policy.collector_exceed_cpu_usage_alert) == 0
    error_message = "Created alarm"
  }
  assert {
    condition     = length(google_monitoring_alert_policy.collector_startup_error_alert) == 0
    error_message = "Created alarm"
  }
  assert {
    condition     = length(google_monitoring_alert_policy.collector_crash_error_alert) == 0
    error_message = "Created alarm"
  }
  assert {
    condition     = length(google_monitoring_alert_policy.export_metric_to_collector_error_alert) == 0
    error_message = "Created alarm"
  }
}

run "creates_cpu_exceed_alarm" {
  command = plan

  variables {
    collector_exceed_cpu_usage_alarm = {
      alignment_period_sec = 0
      auto_close_sec       = 0
      duration_sec         = 0
      enable_alarm         = true
      severity             = ""
      threshold            = 0
    }
  }

  assert {
    condition     = length(google_monitoring_alert_policy.collector_exceed_cpu_usage_alert) == 1
    error_message = "Didn't create alarm"
  }
  assert {
    condition     = strcontains(google_monitoring_alert_policy.collector_exceed_cpu_usage_alert[0].conditions[0].condition_threshold[0].filter, ".*collector.*")
    error_message = "Doesn't alert on proper instance group"
  }
}

run "creates_run_error_alarm" {
  command = plan

  variables {
    collector_startup_error_alarm = {
      alignment_period_sec = 0
      auto_close_sec       = 0
      duration_sec         = 0
      enable_alarm         = true
      severity             = ""
      threshold            = 0
    }
  }

  assert {
    condition     = length(google_monitoring_alert_policy.collector_startup_error_alert) == 1
    error_message = "Didn't create alarm"
  }
  assert {
    condition     = strcontains(google_monitoring_alert_policy.collector_startup_error_alert[0].conditions[0].condition_threshold[0].filter, "environment-collector-startup-error-counter")
    error_message = "Doesn't alert on proper instance group"
  }
}

run "creates_crash_error_alarm" {
  command = plan

  variables {
    collector_crash_error_alarm = {
      alignment_period_sec = 0
      auto_close_sec       = 0
      duration_sec         = 0
      enable_alarm         = true
      severity             = ""
      threshold            = 0
    }
  }

  assert {
    condition     = length(google_monitoring_alert_policy.collector_crash_error_alert) == 1
    error_message = "Didn't create alarm"
  }
  assert {
    condition     = strcontains(google_monitoring_alert_policy.collector_crash_error_alert[0].conditions[0].condition_threshold[0].filter, "environment-collector-crash-error-counter")
    error_message = "Doesn't alert on proper instance group"
  }
}

run "creates_export_metric_alarm" {
  command = plan

  variables {
    export_metric_to_collector_error_alarm = {
      alignment_period_sec = 0
      auto_close_sec       = 0
      duration_sec         = 0
      enable_alarm         = true
      severity             = ""
      threshold            = 0
    }
  }

  assert {
    condition     = length(google_monitoring_alert_policy.export_metric_to_collector_error_alert) == 1
    error_message = "Didn't create alarm"
  }
  assert {
    condition     = strcontains(google_monitoring_alert_policy.export_metric_to_collector_error_alert[0].conditions[0].condition_threshold[0].filter, "environment-server-export-metric-error-counter")
    error_message = "Doesn't alert on proper instance group"
  }
}

run "creates_memory_exceed_alarm" {
  command = plan

  variables {
    collector_exceed_memory_usage_alarm = {
      alignment_period_sec = 0
      auto_close_sec       = 0
      duration_sec         = 0
      enable_alarm         = true
      severity             = ""
      threshold            = 0
    }
  }

  assert {
    condition     = length(google_monitoring_alert_policy.collector_exceed_memory_usage_alarm) == 1
    error_message = "Didn't create alarm"
  }
  assert {
    condition     = strcontains(google_monitoring_alert_policy.collector_exceed_memory_usage_alarm[0].conditions[0].condition_threshold[0].filter, "otelcol_process_memory_rss")
    error_message = "Doesn't alert on proper instance group"
  }
}
