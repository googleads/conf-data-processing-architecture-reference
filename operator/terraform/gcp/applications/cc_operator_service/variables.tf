/**
 * Copyright 2023-2025 Google LLC
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

################################################################################
# Global Variables.
################################################################################

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production"
  type        = string
}

variable "region" {
  description = "Region where all services will be created."
  type        = string
}

variable "region_zone" {
  description = "Region zone where all services will be created."
  type        = string
}

variable "operator_package_bucket_location" {
  description = "Location for operator packages. Example: 'US'"
  type        = string
}

variable "auto_create_subnetworks" {
  description = "When enabled, the network will create a subnet for each region automatically across the 10.128.0.0/9 address range."
  type        = bool
  default     = false
}

variable "network_name_suffix" {
  description = "The suffix of the name of the VPC network of this module. The network name is a combination of environment name and this suffix. This is required if auto_create_subnetworks is disabled."
  type        = string
  default     = "network-with-custom-subnet"
}

variable "worker_subnet_cidr" {
  description = "The range of internal addresses that are owned by worker subnet.Map with Key: region and Value: cidr range."
  type        = map(string)
  default     = {}
}

variable "collector_subnet_cidr" {
  description = "The range of internal addresses that are owned by collector subnet. Map with Key: region and Value: cidr range."
  type        = map(string)
  default     = {}
}

variable "proxy_subnet_cidr" {
  description = "The range of internal addresses that are owned by proxy subnet. Map with Key: region and Value: cidr range."
  type        = map(string)
  default     = {}
}

################################################################################
# Global Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for services."
  type        = bool
  default     = false
}

variable "custom_metrics_alarms_enabled" {
  description = "Enable alarms for custom metrics."
  type        = bool
  default     = false
}

variable "alarms_notification_email" {
  description = "Email to receive alarms for services."
  type        = string
  default     = ""
}

################################################################################
# Common Spanner Variables.
################################################################################

variable "spanner_instance_config" {
  description = "Config value for the Spanner Instance"
  type        = string
}

variable "spanner_processing_units" {
  description = "Number of processing units allocated to the jobmetadata instance. 1000 processing units = 1 node and must be set as a multiple of 100."
  type        = number
  default     = 1000
}

variable "spanner_database_deletion_protection" {
  description = "Prevents destruction of the Spanner database."
  type        = bool
  default     = true
}

################################################################################
# Worker Variables.
################################################################################

variable "instance_type" {
  description = "GCE instance type for worker."
  type        = string
  default     = "n2d-standard-2"
}

variable "worker_instance_disk_type" {
  description = "The worker instance disk type."
  type        = string
  default     = null
}

variable "worker_instance_disk_size_gb" {
  description = "The size of the worker instance disk image in GB."
  type        = number
  default     = null
}

variable "instance_disk_image_family" {
  description = <<-EOT
    The image family from which to initialize the server instance disk.
    Using this variable, the server will pull the latest image in the family
    automatically. If instance_disk_image is set, instance_disk_image_family
    will be ignored."
  EOT
  type = object({
    image_project = string
    image_family  = string
  })
  default = {
    image_project = "confidential-space-images"
    image_family  = "confidential-space-debug"
  }
}

variable "instance_disk_image" {
  description = "The image from which to initialize the worker instance disk. E.g., projects/confidential-space-images/global/images/confidential-space-251000"
  type        = string
  default     = null
}

variable "worker_logging_enabled" {
  description = "Whether to enable worker logging."
  type        = bool
  default     = false
}

variable "worker_monitoring_enabled" {
  description = "Whether to enable worker monitoring."
  type        = bool
  default     = true
}

variable "worker_memory_monitoring_enabled" {
  description = "Whether to enable worker memory monitoring."
  type        = bool
  default     = false
}

variable "worker_container_log_redirect" {
  description = "Where to output container logs: cloud_logging, serial, true (both), false (neither)."
  type        = string
  default     = "false"
}

variable "worker_image" {
  description = "The worker docker image."
  type        = string
}

variable "worker_image_signature_repos" {
  description = <<-EOT
    A list of comma-separated container repositories that store the worker image signatures
    that are generated by Sigstore Cosign.
    Example: "us-docker.pkg.dev/projectA/repo/example,us-docker.pkg.dev/projectB/repo/example".
  EOT
  type        = string
  default     = ""
}

variable "worker_restart_policy" {
  description = "The TEE restart policy. Currently only supports Never"
  type        = string
  default     = "Never"
}

variable "allowed_operator_service_account" {
  description = "The service account provided by coordinator for operator worker to impersonate."
  type        = string
  default     = ""
}

variable "user_provided_worker_sa_email" {
  description = "User provided service account email for worker."
  type        = string
  default     = ""
}

variable "worker_instance_force_replace" {
  description = "Whether to force worker instance replacement for every deployment"
  type        = bool
  default     = false
}

variable "worker_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
  default     = "60"
}

variable "worker_alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = string
  default     = "60"
}

################################################################################
# Autoscaling Variables.
################################################################################

variable "min_worker_instances" {
  description = "Minimum number of instances in worker managed instance group."
  type        = number
  default     = 1
}

variable "max_worker_instances" {
  description = "Maximum number of instances in worker managed instance group."
  type        = number
  default     = 20
}

variable "autoscaling_jobs_per_instance" {
  description = "The ratio of jobs to worker instances to scale by."
  type        = number
  default     = 1
}

variable "autoscaling_cloudfunction_use_java21_runtime" {
  description = "Whether to use the Java 21 runtime for the autoscaling cloud function. If false will use Java 11."
  type        = bool
  nullable    = false
  default     = false
}

################################################################################
# Autoscaling Alarm Variables.
################################################################################

variable "autoscaling_alarm_eval_period_sec" {
  description = "Time period (in seconds) for alarm evaluation."
  type        = string
  default     = "60"
}

variable "autoscaling_alarm_duration_sec" {
  description = "Amount of time (in seconds) to wait before sending an alarm. Must be in minute intervals. Example: '60','120'."
  type        = string
  default     = "60"
}

variable "autoscaling_max_vm_instances_ratio_threshold" {
  description = "A decimal ratio of current to maximum allowed VMs, exceeding which sends an alarm. Example: 0.9."
  type        = number
  default     = 0.9
}

# For Scale-in service
variable "autoscaling_cloudfunction_memory_mb" {
  description = "Memory size in MB for autoscaling cloud function."
  type        = number
  default     = 1024
}

variable "worker_scale_in_jar" {
  description = <<-EOT
          Get worker scale in cloud function path. If not provided defaults to locally built jar file.
        Build with `bazel build //operator/terraform/gcp/applications/jobservice:all`.
      EOT
  type        = string
  default     = null
}

variable "worker_scale_in_path" {
  description = <<-EOT
          Optional. Get worker scale in cloud function GCS path.
        Please note the bucket must be created beforehand, and the file name must be .zip extension.
        This application does not do any validations.
  EOT
  type = object({
    bucket_name   = string
    zip_file_name = string
  })
  default = {
    bucket_name   = ""
    zip_file_name = ""
  }
}

variable "termination_wait_timeout_sec" {
  description = <<-EOT
    The instance termination timeout before force terminating (seconds). The value
    should be greater than max_job_processing_time to ensure jobs can complete in
    before instance termination.
  EOT
  type        = string
  default     = "3600"
}

variable "worker_scale_in_cron" {
  description = "The cron schedule for the worker scale-in scheduler."
  type        = string
  default     = "*/5 * * * *"
}

variable "asg_instances_table_ttl_days" {
  description = "The number of days to retain AsgInstances table records."
  type        = number
  validation {
    condition     = var.asg_instances_table_ttl_days > 0
    error_message = "Must be greater than 0."
  }
  default = 3
}

variable "autoscaling_cloudfunction_5xx_threshold" {
  description = "5xx error counts greater than this value will send an alarm."
  type        = number
  default     = 0
}

variable "autoscaling_cloudfunction_error_threshold" {
  description = "Error counts greater than this value will send an alarm."
  type        = number
  default     = 0
}

variable "autoscaling_cloudfunction_max_execution_time_ms" {
  description = "Max execution time (in ms) allowed before sending an alarm."
  type        = number
  default     = 1000 * 60
}

variable "autoscaling_cloudfunction_alarm_eval_period_sec" {
  description = "Time period (in seconds) for cloudfunction alarm evaluation."
  type        = string
  default     = "60"
}

variable "autoscaling_cloudfunction_alarm_duration_sec" {
  description = "Amount of time (in seconds) to wait before sending a cloudfunction alarm. Must be in minute intervals. Example: '60','120'."
  type        = string
  default     = "60"
}

################################################################################
# Job Queue Alarm Variables.
################################################################################

variable "jobqueue_alarm_eval_period_sec" {
  description = "Time period (in seconds) for alarm evaluation."
  type        = string
  default     = "60"
}

variable "jobqueue_max_undelivered_message_age_sec" {
  description = "Maximum time (in seconds) to wait for message delivery before triggering alarm."
  type        = number
  default     = 60 * 60
}

################################################################################
# VPC Service Control Variables.
################################################################################

variable "vpcsc_compatible" {
  description = <<EOT
      Enable VPC Service Control compatible features:
      * Serverless VPC Access connectors for all Cloud Function functions.
    EOT
  type        = bool
  default     = false
}

variable "vpc_connector_machine_type" {
  description = "Machine type of Serverless VPC Access connectors."
  type        = string
  default     = "e2-micro"
}

################################################################################
# Frontend Service Cloud Run Variables.
################################################################################

variable "frontend_service_cloud_run_regions" {
  description = "The regions to deploy the Cloud Run FE handlers in."
  type        = set(string)
  nullable    = false
  default     = []
}

variable "frontend_service_cloud_run_deletion_protection" {
  description = "Whether to prevent the instance from being deleted by terraform during apply."
  type        = bool
  nullable    = false
  default     = false # CRs might need to be replaced during normal TF apply
}

variable "frontend_service_cloud_run_source_container_image_url" {
  description = "The URL for the container image to run on this service."
  type        = string
  nullable    = false
  default     = ""
}

variable "frontend_service_cloud_run_cpu_idle" {
  description = "Determines whether the CPU is always allocated (false) or if only allocated for usage (true)."
  type        = bool
  nullable    = false
  default     = true # Allocate only when used
}

variable "frontend_service_cloud_run_startup_cpu_boost" {
  description = "Whether to over-allocate CPU for faster new instance startup."
  type        = bool
  nullable    = false
  default     = true # Always boost startup
}

variable "frontend_service_cloud_run_ingress_traffic_setting" {
  description = "Which type of traffic to allow. Options are INGRESS_TRAFFIC_ALL INGRESS_TRAFFIC_INTERNAL_ONLY, INGRESS_TRAFFIC_INTERNAL_LOAD_BALANCER"
  type        = string
  nullable    = false
  default     = "INGRESS_TRAFFIC_ALL"
}

variable "frontend_service_cloud_run_allowed_invoker_iam_members" {
  description = "The identities allowed to invoke this cloud run service. Require the GCP IAM prefixes such as serviceAccount: or user:"
  type        = set(string)
  nullable    = false
  default     = []
}

variable "frontend_service_cloud_run_binary_authorization" {
  description = "Binary Authorization config."
  type = object({
    breakglass_justification = optional(bool)
    use_default              = optional(bool)
    policy                   = optional(string)
  })
  nullable = true
  default  = null
}

variable "frontend_service_cloud_run_custom_audiences" {
  description = "The full URL to be used as a custom audience for invoking this Cloud Run."
  type        = set(string)
  nullable    = false
  default     = []
}

################################################################################
# Frontend Service Load Balancer Variables.
################################################################################

variable "frontend_service_enable_lb_backend_logging" {
  description = "Whether to enable logging for the load balancer traffic served by the backend service."
  type        = bool
  nullable    = false
  default     = false
}

variable "frontend_service_lb_allowed_request_paths" {
  description = "The requests paths that will be forwarded to the backend."
  type        = set(string)
  nullable    = false
  default     = ["/*"]
}

variable "frontend_service_lb_domain" {
  description = "The domain name to use to identify the load balancer. e.g. my.service.com. Will be used ot create a new cert."
  type        = string
  nullable    = true
  default     = null
}

variable "frontend_service_lb_outlier_detection_interval_seconds" {
  description = "Time interval between ejection sweep analysis. This can result in both new ejections as well as hosts being returned to service."
  type        = number
  nullable    = false
  default     = 10
}

variable "frontend_service_lb_outlier_detection_base_ejection_time_seconds" {
  description = "The base time that a host is ejected for. The real time is equal to the base time multiplied by the number of times the host has been ejected."
  type        = number
  nullable    = false
  default     = 120
}

variable "frontend_service_lb_outlier_detection_consecutive_errors" {
  description = "Number of errors before a host is ejected from the connection pool. When the backend host is accessed over HTTP, a 5xx return code qualifies as an error."
  type        = number
  nullable    = false
  default     = 2
}

variable "frontend_service_lb_outlier_detection_enforcing_consecutive_errors" {
  description = "The percentage chance that a host will be actually ejected when an outlier status is detected through consecutive 5xx. This setting can be used to disable ejection or to ramp it up slowly."
  type        = number
  nullable    = false
  default     = 100
}

variable "frontend_service_lb_outlier_detection_consecutive_gateway_failure" {
  description = "The number of consecutive gateway failures (502, 503, 504 status or connection errors that are mapped to one of those status codes) before a consecutive gateway failure ejection occurs."
  type        = number
  nullable    = false
  default     = 2
}

variable "frontend_service_lb_outlier_detection_enforcing_consecutive_gateway_failure" {
  description = "The percentage chance that a host will be actually ejected when an outlier status is detected through consecutive gateway failures. This setting can be used to disable ejection or to ramp it up slowly."
  type        = number
  nullable    = false
  default     = 100
}

variable "frontend_service_lb_outlier_detection_max_ejection_percent" {
  description = "Maximum percentage of hosts in the load balancing pool for the backend service that can be ejected."
  type        = number
  nullable    = false
  default     = 60
}

################################################################################
# Frontend Service Domain Variables.
################################################################################

variable "frontend_service_parent_domain_name" {
  description = "The parent domain name used for the DNS record for the FE e.g. 'my.domain.com'."
  type        = string
  nullable    = false
  default     = ""
}

variable "frontend_service_parent_domain_name_project_id" {
  description = "The ID of the project where the DNS hosted zone for the parent domain exists."
  type        = string
  nullable    = false
  default     = ""
}

################################################################################
# Frontend Service Cloud Function Variables.
################################################################################

variable "create_frontend_service_cloud_function" {
  description = "Whether to create the FE cloud function."
  type        = bool
  default     = true
}

variable "frontend_service_jar" {
  description = <<-EOT
          Get frontend service cloud function path. If not provided defaults to locally built jar file.
        Build with `bazel build //operator/terraform/gcp/applications/jobservice:all`.
      EOT
  type        = string
  default     = null
}

variable "frontend_service_path" {
  description = <<-EOT
          Optional. Get frontend service in cloud function GCS path.
        Please note the bucket must be created beforehand, and the file name must be .zip extension.
        This application does not do any validations.
  EOT
  type = object({
    bucket_name   = string
    zip_file_name = string
  })
  default = {
    bucket_name   = ""
    zip_file_name = ""
  }
}


variable "frontend_service_cloudfunction_num_cpus" {
  description = "The number of CPU to use for frontend service cloud function."
  type        = number
  default     = null
}

variable "frontend_service_cloudfunction_memory_mb" {
  description = "Memory size in MB for frontend service cloud function."
  type        = number
  default     = 1024
}

variable "frontend_service_cloudfunction_min_instances" {
  description = "The minimum number of frontend function instances that may coexist at a given time."
  type        = number
  default     = 0
}

variable "frontend_service_cloudfunction_max_instances" {
  description = "The maximum number of frontend function instances that may coexist at a given time."
  type        = number
  default     = 100
}

variable "frontend_service_cloudfunction_max_instance_request_concurrency" {
  description = "The maximum number of concurrent requests that frontend function instances can receive."
  type        = number
  default     = 1
}

variable "frontend_service_cloudfunction_timeout_sec" {
  description = "Number of seconds after which a frontend function instance times out."
  type        = number
  default     = 60
}

variable "frontend_service_cloudfunction_runtime_sa_email" {
  description = "Email of the service account to use as the runtime identity of the FE service."
  type        = string
  nullable    = true
  default     = null
}

variable "frontend_service_cloudfunction_use_java21_runtime" {
  description = "Whether to use the Java 21 runtime for the frontend service cloud function. If false will use Java 11."
  type        = bool
  nullable    = false
  default     = false
}

variable "job_table_ttl_days" {
  description = "The number of days to retain job table records."
  type        = number
  validation {
    condition     = var.job_table_ttl_days > 0
    error_message = "Must be greater than 0."
  }
  default = 365
}

variable "job_version" {
  description = "The version of frontend service. Version 2 supports new job schema from C++ CMRT library."
  type        = string
  default     = "2"
}

################################################################################
# Frontend Service Alarm Variables.
################################################################################

variable "frontend_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
  default     = "60"
}

variable "frontend_alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = string
  default     = "60"
}

variable "frontend_cloudfunction_error_threshold" {
  description = "Error count greater than this to send alarm. Example: 0."
  type        = string
  default     = "0"
}

variable "frontend_cloudfunction_max_execution_time_max" {
  description = "Max execution time in ms to send alarm. Example: 9999."
  type        = string
  default     = "5000"
}

variable "frontend_cloudfunction_5xx_threshold" {
  description = "Cloud Function 5xx error count greater than this to send alarm. Example: 0."
  type        = string
  default     = "0"
}

variable "frontend_lb_max_latency_ms" {
  description = "Load Balancer max latency to send alarm. Measured in milliseconds. Example: 5000."
  type        = string
  default     = "5000"
}

variable "frontend_lb_5xx_threshold" {
  description = "Load Balancer 5xx error count greater than this to send alarm. Example: 0."
  type        = string
  default     = "0"
}

variable "frontend_cloud_run_error_5xx_alarm_config" {
  description = "The configuration for the Cloud Run 5xx error alarm."

  type = object({
    enable_alarm    = bool   # Whether to enable this alarm
    eval_period_sec = number # Amount of time (in seconds) for alarm evaluation. Example: '60'
    duration_sec    = number # Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'
    error_threshold = number # error count greater than this to send alarm. Example: 0.
  })

  default = null
}

variable "frontend_cloud_run_non_5xx_error_alarm_config" {
  description = "The configuration for the Cloud Run non-5xx error (3xx-4xx) alarm."

  type = object({
    enable_alarm    = bool   # Whether to enable this alarm
    eval_period_sec = number # Amount of time (in seconds) for alarm evaluation. Example: '60'
    duration_sec    = number # Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'
    error_threshold = number # error count greater than this to send alarm. Example: 500.
  })

  default = null
}

variable "frontend_cloud_run_execution_time_alarm_config" {
  description = "The configuration for the Cloud Run execution time alarm."

  type = object({
    enable_alarm    = bool   # Whether to enable this alarm
    eval_period_sec = number # Amount of time (in seconds) for alarm evaluation. Example: '60'
    duration_sec    = number # Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'
    threshold_ms    = number # Execution times greater than this to send alarm. Example: 0.
  })

  default = null
}

variable "frontend_lb_error_5xx_alarm_config" {
  description = "The configuration for the Load Balancer 5xx error alarm."

  type = object({
    enable_alarm    = bool   # Whether to enable this alarm
    eval_period_sec = number # Amount of time (in seconds) for alarm evaluation. Example: '60'
    duration_sec    = number # Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'
    error_threshold = number # error count greater than this to send alarm. Example: 0.
  })

  default = {
    enable_alarm    = false
    eval_period_sec = 60
    duration_sec    = 60
    error_threshold = 10
  }
}

variable "frontend_lb_non_5xx_error_alarm_config" {
  description = "The configuration for the Load Balancer non-5xx error (3xx-4xx) alarm."

  type = object({
    enable_alarm    = bool   # Whether to enable this alarm
    eval_period_sec = number # Amount of time (in seconds) for alarm evaluation. Example: '60'
    duration_sec    = number # Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'
    error_threshold = number # error count greater than this to send alarm. Example: 500.
  })

  default = {
    enable_alarm    = false
    eval_period_sec = 60
    duration_sec    = 60
    error_threshold = 50
  }
}

variable "frontend_lb_request_latencies_alarm_config" {
  description = "The configuration for the Load Balancer request latency alarm."

  type = object({
    enable_alarm    = bool   # Whether to enable this alarm
    eval_period_sec = number # Amount of time (in seconds) for alarm evaluation. Example: '60'
    duration_sec    = number # Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'
    threshold_ms    = number # Request latencies greater than this to send alarm. Example: 0.
  })

  default = {
    enable_alarm    = false
    eval_period_sec = 60
    duration_sec    = 60
    threshold_ms    = 60000
  }
}

################################################################################
# Worker Alert thresholds
################################################################################

variable "joblifecyclehelper_job_pulling_failure_threshold" {
  description = "Number of job pulling failures within alarm_eval_period_sec to trigger the alert."
  type        = number
  default     = 1
}

variable "joblifecyclehelper_job_completion_failure_threshold" {
  description = "Number of job completion failures within alarm_eval_period_sec to trigger the alert."
  type        = number
  default     = 1
}

variable "joblifecyclehelper_job_extender_failure_threshold" {
  description = "Number of job extender failures within alarm_eval_period_sec to trigger the alert."
  type        = number
  default     = 1
}

variable "joblifecyclehelper_job_pool_failure_threshold" {
  description = "Number of job pool failures within alarm_eval_period_sec to trigger the alert."
  type        = number
  default     = 1
}

variable "joblifecyclehelper_job_processing_time_threshold" {
  description = "Time limit in ms for job processing per each job to trigger the alert."
  type        = number
  default     = 1500
}

variable "joblifecyclehelper_job_release_failure_threshold" {
  description = "Number of job release failures within alarm_eval_period_sec to trigger the alert."
  type        = number
  default     = 1
}

variable "joblifecyclehelper_job_waiting_time_threshold" {
  description = "Time limit in ms for job waiting per each job to trigger the alert."
  type        = number
  default     = 1000000
}

################################################################################
# OpenTelemetry Collector variables
################################################################################
variable "collector_regional_config" {
  description = "Region and zone level configurations."
  type = map(object({
    zonal_config = map(object({
      min_collector_count              = optional(number, 1)
      max_collector_count              = optional(number, 3)
      collector_cpu_utilization_target = optional(number, 0.8)
    }))
  }))
  default = {}
}

variable "enable_opentelemetry_collector" {
  description = "When true, install the collector module to operator_service."
  type        = bool
  default     = false
}

variable "collector_instance_type" {
  description = "GCE instance type for worker."
  type        = string
  default     = "n2d-standard-2"
}

variable "cos_image_family" {
  description = "COS image family to use."
  type        = string
  default     = "cos-121-lts"
}

variable "user_provided_collector_sa_email" {
  description = "User provided service account email for OpenTelemetry Collector."
  type        = string
  default     = ""
}

variable "collector_service_port_name" {
  description = "The name of the http port that receives traffic destined for the OpenTelemetry collector."
  type        = string
  default     = "otlp"
}

variable "collector_service_port" {
  description = "The value of the http port that receives traffic destined for the OpenTelemetry collector."
  type        = number
  default     = 4318
}

variable "collector_domain_name" {
  description = "The dns domain name for OpenTelemetry collector."
  type        = string
  default     = "collector.metrics"
}

variable "collector_dns_name" {
  description = "The dns name for OpenTelemetry collector."
  type        = string
  default     = "scptestings.dev"
}

variable "otel_collector_startup_config" {
  description = "Startup configuration for the OpenTelemetry collector."
  type = object({
    otel_collector_image_uri = optional(string, "otel/opentelemetry-collector-contrib:0.122.1")
    metric_prefix            = optional(string, "custom.googleapis.com")
    send_batch_max_size      = optional(number, 200)
    send_batch_size          = optional(number, 200)
    send_batch_timeout       = optional(string, "5s")
    collector_queue_size     = optional(number, 5000)
  })
  default = {}
}

variable "collector_min_instance_ready_sec" {
  description = "Waiting time for the new instance to be ready."
  type        = number
  default     = 120
}

variable "collector_exceed_cpu_usage_alarm" {
  description = "Configuration for the exceed CPU usage alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 0.9,
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "collector_exceed_memory_usage_alarm" {
  description = "Configuration for the exceed memory usage alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 6442450944, # 6 GB
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "collector_startup_error_alarm" {
  description = "Configuration for the collector startup error alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 50,
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "collector_crash_error_alarm" {
  description = "Configuration for the collector crash error alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 50,
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "export_metric_to_collector_error_alarm" {
  description = "Configuration for the server exporting metrics error alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 50,
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "collector_queue_size_alarm" {
  description = "Configuration for the collector queue size alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 0.8,
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "collector_send_metric_failure_rate_alarm" {
  description = "Configuration for the collector send metric failure alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 0.05,
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "collector_refuse_metric_rate_alarm" {
  description = "Configuration for the collector refuse metric alarm."
  type = object({
    enable_alarm : bool,
    duration_sec : number,
    alignment_period_sec : number,
    threshold : number,
    severity : string,
    auto_close_sec : number
  })
  default = {
    enable_alarm : false,
    duration_sec : 300,
    alignment_period_sec : 600,
    threshold : 0.05,
    severity : "moderate",
    auto_close_sec : 1800
  }
}

variable "collector_dns_zone" {
  description = "Google Cloud DNS zone name for collector."
  type        = string
  default     = ""
}
