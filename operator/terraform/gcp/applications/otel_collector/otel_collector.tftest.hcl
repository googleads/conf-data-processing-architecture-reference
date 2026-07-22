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

# All run blocks should have "command = plan".
# Take great care when writing tests with "command = apply".
variables {
  project_id            = "test-project"
  environment           = "test-env"
  region                = "us-central1"
  region_zone           = "us-central1-a"
  network               = "test-network"
  internet_tag_for_otel = "egress-internet"
  collector_subnet_id   = "test-collector-id"
  proxy_subnet_id       = "test-proxy-id"
  subnets_per_region = {
    us-central1 = "test-collector-id",
    us-east1    = "test-collector-id-2"
  }
  proxy_only_subnets_per_region = {
    us-central1 = "test-proxy-id",
    us-east1    = "test-proxy-id-2"
  }
  collector_regional_config = {
    us-central1 = {
      zonal_config = {
        us-central1-a = {
          min_collector_count              = 1
          max_collector_count              = 3
          collector_cpu_utilization_target = 0.8
        }
      }
    }
    us-east1 = {
      zonal_config = {
        us-east1-a = {
          min_collector_count              = 1
          max_collector_count              = 3
          collector_cpu_utilization_target = 0.8
        }
      }
    }
  }
}

run "creates_otel_collector" {
  command = plan

  providers = {
    google = google
  }

  assert {
    condition     = module.otel_collector != null
    error_message = "OpenTelemetry collector module not created"
  }

  assert {
    condition     = module.otel_collector_load_balancer != null
    error_message = "OpenTelemetry collector load balancer module not created"
  }
}

run "generates_outputs_with_plan" {
  command = plan

  assert {
    condition     = length(output.forwarding_rules) == length(var.subnets_per_region)
    error_message = "Wrong rule"
  }
  assert {
    condition     = output.http_proxy.name != ""
    error_message = "Wrong proxy"
  }
  assert {
    condition     = output.loadbalancer_dns_address != ""
    error_message = "Wrong address"
  }
}