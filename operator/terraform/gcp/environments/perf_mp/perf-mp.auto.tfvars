/**
 * Copyright 2022 Google LLC
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

environment = "perf-mp"
project_id  = "admcloud-adtech1"
region      = "us-central1"
region_zone = "us-central1-c"

# Multi region location
# https://cloud.google.com/storage/docs/locations
operator_package_bucket_location = "US"

spanner_instance_config              = "regional-us-central1"
spanner_processing_units             = 100
spanner_database_deletion_protection = false

worker_image                     = "us-docker.pkg.dev/admcloud-scp/docker-repo-dev/worker_app_mp_gcp:perf"
allowed_operator_service_account = "staging-a-opallowedusr@admcloud-coordinator1.iam.gserviceaccount.com,staging-b-opallowedusr@admcloud-coordinator2.iam.gserviceaccount.com"

worker_logging_enabled = true
instance_disk_image    = "confidential-space-images/confidential-space"

user_provided_worker_sa_email = "perf-mp-worker-account@admcloud-adtech1.iam.gserviceaccount.com"
