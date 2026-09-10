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
  internet_tag_for_otel            = "egress-internet"
  subnet_id                        = ""
  region                           = ""
  user_provided_collector_sa_email = ""
  collector_instance_type          = ""
  cos_image_family                 = "cos-121-lts"
  collector_startup_script         = "script"
  collector_service_port           = 0
  collector_service_port_name      = "port_name"
  collector_min_instance_ready_sec = 0
  collector_regional_config = {
    "us-central1" = {
      zonal_config = {
        "us-central1-c" = {
        }
      }
    },
    "us-east1" = {
      zonal_config = {
        "us-east1-c" = {
          min_collector_count              = 2
          max_collector_count              = 4
          collector_cpu_utilization_target = 0.5
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

run "creates_server_replace_trigger" {
  command = plan

  assert {
    condition = null_resource.server_instance_replace_trigger.triggers == tomap({
      replace = join(".", [sha256("script"), "google_service_account_email"])
    })
    error_message = "Wrong trigger"
  }
}

run "creates_template_mig_trigger_with_empty_string" {
  command = plan

  variables {
    network    = ""
    project_id = ""
    subnets_per_region = {
      "us-east1"    = ""
      "us-central1" = ""
    }
  }

  assert {
    condition = null_resource.collector_template_mig_replace_trigger["us-east1-c"].triggers == tomap({
      network   = ""
      subnet_id = ""
      zone      = "us-east1-c"
    })
    error_message = "Wrong trigger"
  }
}

run "creates_template_mig_trigger_with_network_and_subnet" {
  command = plan

  variables {
    network    = "network"
    project_id = "project_id"
  }

  assert {
    condition = null_resource.collector_template_mig_replace_trigger["us-east1-c"].triggers == tomap({
      network   = "network"
      subnet_id = "subnet_id2"
      zone      = "us-east1-c"
    })
    error_message = "Wrong trigger"
  }
  assert {
    condition = null_resource.collector_template_mig_replace_trigger["us-central1-c"].triggers == tomap({
      network   = "network"
      subnet_id = "subnet_id1"
      zone      = "us-central1-c"
    })
    error_message = "Wrong trigger"
  }
}

run "creates_service_account" {
  command = plan

  assert {
    condition     = length(google_service_account.collector_service_account) == 1
    error_message = "Didn't create service account"
  }
}

run "doesnt_create_service_account" {
  command = plan

  variables {
    user_provided_collector_sa_email = "email"
  }

  assert {
    condition     = length(google_service_account.collector_service_account) == 0
    error_message = "Created service account"
  }
  assert {
    condition     = length(google_project_iam_member.collector_service_account_monitoring_viewer) == 0
    error_message = "Created IAM member"
  }
  assert {
    condition     = length(google_project_iam_member.collector_service_account_metric_writer_iam) == 0
    error_message = "Created IAM member"
  }
  assert {
    condition     = length(google_project_iam_member.collector_service_account_log_writer_iam) == 0
    error_message = "Created IAM member"
  }
}

run "iam_members_use_autocreated_account" {
  command = plan

  providers = {
    google      = google
    google-beta = google-beta
  }

  assert {
    condition     = google_project_iam_member.collector_service_account_monitoring_viewer[0].member == "serviceAccount:google_service_account_email"
    error_message = "IAM member using wrong member"
  }
  assert {
    condition     = google_project_iam_member.collector_service_account_metric_writer_iam[0].member == "serviceAccount:google_service_account_email"
    error_message = "IAM member using wrong member"
  }
  assert {
    condition     = google_project_iam_member.collector_service_account_log_writer_iam[0].member == "serviceAccount:google_service_account_email"
    error_message = "IAM member using wrong member"
  }
}

run "ig_manager_uses_proper_autohealing_and_template" {
  command = plan

  assert {
    condition     = google_compute_instance_group_manager.collector_instance_groups["us-central1-c"].auto_healing_policies[0].health_check == "health_check_id"
    error_message = "Wrong auto healing"
  }
  assert {
    condition     = google_compute_instance_group_manager.collector_instance_groups["us-central1-c"].version[0].instance_template == "instance_template_id"
    error_message = "Wrong template"
  }
  assert {
    condition     = google_compute_instance_group_manager.collector_instance_groups["us-east1-c"].auto_healing_policies[0].health_check == "health_check_id"
    error_message = "Wrong auto healing"
  }
  assert {
    condition     = google_compute_instance_group_manager.collector_instance_groups["us-east1-c"].version[0].instance_template == "instance_template_id"
    error_message = "Wrong template"
  }
}

run "creates_collector_autoscaler_per_zone" {
  command = plan

  assert {
    condition     = length(google_compute_autoscaler.collector_autoscalers) == 2
    error_message = "Wrong number of autoscaler created, expecting 1 per zone"
  }

  assert {
    condition     = google_compute_autoscaler.collector_autoscalers["us-central1-c"].autoscaling_policy[0].min_replicas == 1
    error_message = "Default min replicas for autoscaler not as expected"
  }
  assert {
    condition     = google_compute_autoscaler.collector_autoscalers["us-central1-c"].autoscaling_policy[0].max_replicas == 3
    error_message = "Default max replicas for autoscaler not as expected"
  }
  assert {
    condition     = google_compute_autoscaler.collector_autoscalers["us-central1-c"].autoscaling_policy[0].cpu_utilization[0].target == 0.8
    error_message = "Default cpu utilization target for autoscaler not as expected"
  }
  assert {
    condition     = google_compute_autoscaler.collector_autoscalers["us-east1-c"].autoscaling_policy[0].min_replicas == 2
    error_message = "Wrong min replicas for autoscaler"
  }
  assert {
    condition     = google_compute_autoscaler.collector_autoscalers["us-east1-c"].autoscaling_policy[0].max_replicas == 4
    error_message = "Wrong max replicas for autoscaler"
  }
  assert {
    condition     = google_compute_autoscaler.collector_autoscalers["us-east1-c"].autoscaling_policy[0].cpu_utilization[0].target == 0.5
    error_message = "Wrong cpu utilization target for autoscaler"
  }
}

run "verify_instance_template_tags" {
  command = plan

  assert {
    condition     = contains(google_compute_instance_template.collector["us-central1"].tags, var.internet_tag_for_otel)
    error_message = "Instance template tags do not contain the expected tag: ${var.internet_tag_for_otel}"
  }
}

run "verify_instance_template_image" {
  command = plan

  assert {
    condition     = data.google_compute_image.cos_image.family == "cos-121-lts"
    error_message = "cos_image data source does not use the expected cos_image_family"
  }

  assert {
    condition     = data.google_compute_image.cos_image.project == "cos-cloud"
    error_message = "cos_image data source does not use the expected cos-cloud project"
  }

  assert {
    condition     = google_compute_instance_template.collector["us-central1"].disk[0].source_image == "google_compute_image_self_link"
    error_message = "Instance template does not use the dynamic cos_image data source"
  }
}

run "verify_instance_template_custom_image_family" {
  command = plan

  variables {
    cos_image_family = "custom-cos-family"
  }

  assert {
    condition     = data.google_compute_image.cos_image.family == "custom-cos-family"
    error_message = "cos_image data source does not use the configured cos_image_family"
  }

  assert {
    condition     = google_compute_instance_template.collector["us-central1"].disk[0].source_image == "google_compute_image_self_link"
    error_message = "Instance template does not use the dynamic cos_image data source"
  }
}
