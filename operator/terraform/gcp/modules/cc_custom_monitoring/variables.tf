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

################################################################################
# Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for worker job processor"
  type        = bool
}

variable "environment" {
  description = "Environment where this service is deployed (e.g. dev, prod)."
  type        = string
}

variable "vm_instance_group_name" {
  description = "Name for the instance group for the worker VM."
  type        = string
}

variable "alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = string
}

variable "notification_channel_id" {
  description = "Notification channel to which to send alarms."
  type        = string
}

################################################################################
# Alert thresholds
################################################################################

variable "joblifecyclehelper_job_pulling_failure_threshold" {
  description = "Number of job pulling failures within alarm_eval_period_sec to trigger the alert."
  type        = number
}

variable "joblifecyclehelper_job_completion_failure_threshold" {
  description = "Number of job completion failures within alarm_eval_period_sec to trigger the alert."
  type        = number
}

variable "joblifecyclehelper_job_extender_failure_threshold" {
  description = "Number of job extender failures within alarm_eval_period_sec to trigger the alert."
  type        = number
}

variable "joblifecyclehelper_job_pool_failure_threshold" {
  description = "Number of job pool failures within alarm_eval_period_sec to trigger the alert."
  type        = number
}

variable "joblifecyclehelper_job_processing_time_threshold" {
  description = "Time limit in ms for job processing per each job to trigger the alert."
  type        = number
}

variable "joblifecyclehelper_job_release_failure_threshold" {
  description = "Number of job release failures within alarm_eval_period_sec to trigger the alert."
  type        = number
}

variable "joblifecyclehelper_job_waiting_time_threshold" {
  description = "Time limit in ms for job waiting per each job to trigger the alert."
  type        = number
}
