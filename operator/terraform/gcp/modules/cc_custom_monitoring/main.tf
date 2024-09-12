/**
 * Copyright 2024 Google LLC
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

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pulling_metric" {
  display_name = "Job LifecycleHelper Job Pulling Rate"
  description  = "Custom metric for job pulling Rate in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobPreparationCount"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Count"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pulling_failure_metric" {
  display_name = "Job LifecycleHelper Job Pulling Failures"
  description  = "Custom metric for job pulling failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobPreparationFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_alert_policy" "joblifecyclehelper_job_pulling_failure_alert" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Job LifecycleHelper Job Pulling Failure Alert"
  combiner     = "OR"
  conditions {
    display_name = "Job Pulling Failures"
    condition_threshold {
      filter          = "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_pulling_failure_metric.type}\" AND resource.type=\"gce_instance\""
      duration        = "${var.alarm_duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.joblifecyclehelper_job_pulling_failure_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period   = "${var.alarm_eval_period_sec}s"
        per_series_aligner = "ALIGN_SUM"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
  alert_strategy {
    auto_close = "604800s"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pulling_try_finish_instance_termination_failure_metric" {
  display_name = "Job LifecycleHelper Job Pulling Try FinishInstance Termination Failures"
  description  = "Custom metric for try finish instance termination failures in Job Pulling in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobPreparationTryFinishInstanceTerminationFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pulling_current_instance_termination_failure_metric" {
  display_name = "Job LifecycleHelper Job Pulling Current Instance Termination Failures"
  description  = "Custom metric for current instance termination failures in Job Pulling in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobPreparationCurrentInstanceTerminationFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pulling_get_next_job_failure_metric" {
  display_name = "Job LifecycleHelper Job Pulling Get Next Job Failures"
  description  = "Custom metric for get next job failures in Job Pulling in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobPreparationGetNextJobFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pulling_update_job_status_failure_metric" {
  display_name = "Job LifecycleHelper Job Pulling Update Job Status Failures"
  description  = "Custom metric for update job status in Job Pulling in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobPreparationUpdateJobStatusFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pulling_delete_orphaned_job_message_failure_metric" {
  display_name = "Job LifecycleHelper Job Pulling Delete Orphaned Job Message Failures"
  description  = "Custom metric for delete orphaned job message failures in Job Pulling in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobPreparationDeleteOrphanedJobMessageFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_completion_metric" {
  display_name = "Job LifecycleHelper Job Completion Rate"
  description  = "Custom metric for job completion Rate in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobCompletionCount"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Count"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_completion_failure_metric" {
  display_name = "Job LifecycleHelper Job Completion Failures Rate"
  description  = "Custom metric for job completion failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobCompletionFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_alert_policy" "joblifecyclehelper_job_completion_failure_alert" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Job LifecycleHelper Job Completion Failure Alert"
  combiner     = "OR"
  conditions {
    display_name = "Job Completion Failures"
    condition_threshold {
      filter          = "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_completion_failure_metric.type}\" AND resource.type=\"gce_instance\""
      duration        = "${var.alarm_duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.joblifecyclehelper_job_completion_failure_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period   = "${var.alarm_eval_period_sec}s"
        per_series_aligner = "ALIGN_SUM"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
  alert_strategy {
    auto_close = "604800s"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_completion_invalid_mark_job_completed_failure_metric" {
  display_name = "Job LifecycleHelper Job Completion Invalid Mark Job Completed Failures"
  description  = "Custom metric for invalid mark job completed failures in job completion in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobCompletionInvalidMarkJobCompletedFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_completion_get_job_by_id_failure_metric" {
  display_name = "Job LifecycleHelper Job Completion Get Job By Id Failures"
  description  = "Custom metric for get job by id failures in job completion in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobCompletionGetJobByIdFailureFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_completion_update_job_status_failure_metric" {
  display_name = "Job LifecycleHelper Job Completion Update Job Status Failures"
  description  = "Custom metric for update job status failures in job completion in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobCompletionUpdateJobStatusFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_processing_time_metric" {
  display_name = "Job LifecycleHelper Job Processing Time"
  description  = "Custom metric for job processing time in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobProcessingTimeCount"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "ProcessingTime"
  }
}

resource "google_monitoring_alert_policy" "joblifecyclehelper_job_processing_time_alert" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Job LifecycleHelper Job Processing Time Alert"
  combiner     = "OR"
  conditions {
    display_name = "Job Processing Time Exceeds Limit"
    condition_threshold {
      filter          = "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_processing_time_metric.type}\" AND resource.type=\"gce_instance\""
      duration        = "${var.alarm_duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.joblifecyclehelper_job_processing_time_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period     = "${var.alarm_eval_period_sec}s"
        per_series_aligner   = "ALIGN_MEAN"
        cross_series_reducer = "REDUCE_MEAN"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
  alert_strategy {
    auto_close = "604800s"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_extender_failure_metric" {
  display_name = "Job LifecycleHelper Job Extender Failures"
  description  = "Custom metric for job extender failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobExtenderFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_alert_policy" "joblifecyclehelper_job_extender_failure_alert" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Job LifecycleHelper Job Extender Failure Alert"
  combiner     = "OR"
  conditions {
    display_name = "Job Extender Failures"
    condition_threshold {
      filter          = "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_extender_failure_metric.type}\" AND resource.type=\"gce_instance\""
      duration        = "${var.alarm_duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.joblifecyclehelper_job_extender_failure_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period   = "${var.alarm_eval_period_sec}s"
        per_series_aligner = "ALIGN_SUM"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
  alert_strategy {
    auto_close = "604800s"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_extender_missing_receipt_info_failure_metric" {
  display_name = "Job LifecycleHelper Job Extender Missing Receipt Info Failures"
  description  = "Custom metric for missing receipt info failures in job extender in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobExtenderMissingReceiptInfoFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_extender_update_job_visibility_timeout_failure_metric" {
  display_name = "Job LifecycleHelper Job Extender Update Job Visibility Timeout Failures"
  description  = "Custom metric for update job visibility timeout failures in job extender in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobExtenderUpdateJobVisibilityTimeoutFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_metadata_map_failure_metric" {
  display_name = "Job LifecycleHelper Job Metadata Map Failures"
  description  = "Custom metric for job metadata map failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobMetadataMapFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_alert_policy" "joblifecyclehelper_job_pool_failure_alert" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Job LifecycleHelper Job Pool Failure Alert"
  combiner     = "OR"
  conditions {
    display_name = "Job Pool Failures"
    condition_threshold {
      filter          = "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_metadata_map_failure_metric.type}\" AND resource.type=\"gce_instance\""
      duration        = "${var.alarm_duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.joblifecyclehelper_job_pool_failure_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period   = "${var.alarm_eval_period_sec}s"
        per_series_aligner = "ALIGN_SUM"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
  alert_strategy {
    auto_close = "604800s"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pool_update_job_failure_metric" {
  display_name = "Job LifecycleHelper Job Pool Update Job Failures"
  description  = "Custom metric for update job failures in job pool in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobMetadataMapUpdateJobMetadataFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pool_delete_job__failure_metric" {
  display_name = "Job LifecycleHelper Job Pool Delete Job Failures"
  description  = "Custom metric for delete job failures in job pool in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobMetadataMapDeleteJobMetadataFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pool_find_job_failure_metric" {
  display_name = "Job LifecycleHelper Job Pool Find Job Failures"
  description  = "Custom metric for find job failures in job pool in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobMetadataMapFindJobMetadataFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_pool_insert_job_failure_metric" {
  display_name = "Job LifecycleHelper Job Pool Insert Job Failures"
  description  = "Custom metric for insert job failures in job pool in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobMetadataMapInsertJobMetadataFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_waiting_time_metric" {
  display_name = "Job Waiting Time"
  description  = "Custom metric for job waiting time."

  type        = "custom.googleapis.com/${var.environment}/JobWaitingTimeCount"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Count"
  }
}

resource "google_monitoring_alert_policy" "joblifecyclehelper_job_waiting_time_alert" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Job LifecycleHelper Job Waiting Time Alert"
  combiner     = "OR"
  conditions {
    display_name = "Job Waiting Time Exceeds Limit"
    condition_threshold {
      filter          = "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_waiting_time_metric.type}\" AND resource.type=\"gce_instance\""
      duration        = "${var.alarm_duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.joblifecyclehelper_job_waiting_time_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period     = "${var.alarm_eval_period_sec}s"
        per_series_aligner   = "ALIGN_MEAN"
        cross_series_reducer = "REDUCE_MEAN"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
  alert_strategy {
    auto_close = "604800s"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_release_metric" {
  display_name = "Job Release Rate"
  description  = "Custom metric for job release rate."

  type        = "custom.googleapis.com/${var.environment}/JobReleaseCount"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Count"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_release_failure_metric" {
  display_name = "Job Release Failure Rate"
  description  = "Custom metric for job release failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobReleaseFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_alert_policy" "joblifecyclehelper_job_release_failure_alert" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Job Release Failure Alert"
  combiner     = "OR"
  conditions {
    display_name = "Job Release Failures"
    condition_threshold {
      filter          = "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_release_failure_metric.type}\" AND resource.type=\"gce_instance\""
      duration        = "${var.alarm_duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.joblifecyclehelper_job_release_failure_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period   = "${var.alarm_eval_period_sec}s"
        per_series_aligner = "ALIGN_SUM"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
  alert_strategy {
    auto_close = "604800s"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_release_invalid_release_job_for_retry_failure_metric" {
  display_name = "Job Release Invalid Release Job For Retry Failure Rate"
  description  = "Custom metric for job release invalid release job for retry failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobReleaseInvalidReleaseJobForRetryFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_release_get_job_by_id_failure_metric" {
  display_name = "Job Release Get Job By Id Failure Rate"
  description  = "Custom metric for job release get job by id failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobReleaseGetJobByIdFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_release_invalid_job_status_failure_metric" {
  display_name = "Job Release Invalid Job Status Failure Rate"
  description  = "Custom metric for job release invalid job status failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobReleaseInvalidJobStatusFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_release_update_job_status_failure_metric" {
  display_name = "Job Release Update Job Status Failure Rate"
  description  = "Custom metric for job release update job status failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobReleaseUpdateJobStatusFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_metric_descriptor" "joblifecyclehelper_job_release_update_job_visibility_timeout_failure_metric" {
  display_name = "Job Release Update Job Visibility Timeout Failure Rate"
  description  = "Custom metric for job release update job visibility timeout failures in the job lifecycle helper."

  type        = "custom.googleapis.com/${var.environment}/JobReleaseUpdateJobVisibilityTimeoutFailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Error"
  }
}

resource "google_monitoring_dashboard" "lib_worker_custom_metrics_dashboard" {
  dashboard_json = jsonencode(
    {
      "displayName" : "${var.environment} Lib Worker Custom Metrics Dashboard",
      "dashboardFilters" : [],
      "gridLayout" : {
        "columns" : 2,
        "widgets" : [
          {
            "title" : "Job Pulling and Completion Rate (per minute)",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "breakdowns" : [],
                  "dimensions" : [],
                  "legendTemplate" : "Job Pulling Rate",
                  "measures" : [],
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_pulling_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\" metric.label.\"EventCode\"=\"JobPreparationSuccess\""
                    }
                  }
                },
                {
                  "legendTemplate" : "Job Completion Rate",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "metadata.system_labels.\"instance_group\""
                        ],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_completion_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\" metric.label.\"EventCode\"=\"JobCompletionSuccess\""
                    }
                  }
                }
              ],
              "thresholds" : [],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "Error Rate (per minute)",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "breakdowns" : [],
                  "dimensions" : [],
                  "legendTemplate" : "Job Pulling Failures $${metric.labels.EventCode}",
                  "measures" : [],
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "perSeriesAligner" : "ALIGN_MEAN",
                        "groupByFields" : [
                          "metric.label.\"EventCode\""
                        ]
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_release_failure_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\""
                    }
                  }
                },
                {
                  "breakdowns" : [],
                  "dimensions" : [],
                  "legendTemplate" : "Job Pulling Failures $${metric.labels.EventCode}",
                  "measures" : [],
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "perSeriesAligner" : "ALIGN_MEAN",
                        "groupByFields" : [
                          "metric.label.\"EventCode\""
                        ]
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_pulling_failure_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\""
                    }
                  }
                },
                {
                  "legendTemplate" : "Job Completion Failures $${metric.labels.EventCode}",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "perSeriesAligner" : "ALIGN_MEAN",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "metric.label.\"EventCode\""
                        ]
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_completion_failure_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\""
                    }
                  }
                },
                {
                  "breakdowns" : [],
                  "dimensions" : [],
                  "legendTemplate" : "Job Extender Failures",
                  "measures" : [],
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "metadata.system_labels.\"instance_group\""
                        ],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_extender_failure_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\""
                    }
                  }
                },
                {
                  "breakdowns" : [],
                  "dimensions" : [],
                  "legendTemplate" : "Job MetaData Map Failures $${metric.labels.EventCode}",
                  "measures" : [],
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "metric.label.\"EventCode\""
                        ],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_metadata_map_failure_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\""
                    }
                  }
                }
              ],
              "thresholds" : [],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "Job Processing Time (ms)",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "Job Processing Time",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_processing_time_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "groupByFields" : [],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Job Processing Time - 99th percentile",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_PERCENTILE_99",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_processing_time_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Job Processing Time - 95th percentile",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_PERCENTILE_95",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_processing_time_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                }
              ],
              "thresholds" : [
                {
                  "label" : "",
                  "targetAxis" : "Y2",
                  "value" : 500
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "Job Waiting Time (ms)",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "Job Waiting Time",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_waiting_time_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "groupByFields" : [],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Job Waiting Time - 99th percentile",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_PERCENTILE_99",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_waiting_time_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Job Waiting Time - 95th percentile",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_PERCENTILE_95",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_waiting_time_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                }
              ],
              "thresholds" : [],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "Job Failure Ratio",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "Job Failure Ratio",
                  "plotType" : "LINE",
                  "targetAxis" : "Y1",
                  "timeSeriesQuery" : {
                    "timeSeriesFilterRatio" : {
                      "denominator" : {
                        "aggregation" : {
                          "crossSeriesReducer" : "REDUCE_SUM",
                          "groupByFields" : [],
                          "perSeriesAligner" : "ALIGN_MEAN"
                        },
                        "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_completion_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\" metric.label.\"EventCode\"=\"JobCompletionSuccess\""
                      },
                      "numerator" : {
                        "aggregation" : {
                          "alignmentPeriod" : "60s",
                          "crossSeriesReducer" : "REDUCE_SUM",
                          "groupByFields" : [],
                          "perSeriesAligner" : "ALIGN_MEAN"
                        },
                        "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_completion_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\" metric.label.\"EventCode\"=\"JobCompletionJobStatusFailure\""
                      }
                    }
                  }
                }
              ],
              "thresholds" : [],
              "timeshiftDuration" : "0s",
              "yAxis" : {
                "label" : "",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "Job Release Ratio",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "Job Release Ratio",
                  "plotType" : "LINE",
                  "targetAxis" : "Y1",
                  "timeSeriesQuery" : {
                    "timeSeriesFilterRatio" : {
                      "denominator" : {
                        "aggregation" : {
                          "crossSeriesReducer" : "REDUCE_SUM",
                          "groupByFields" : [],
                          "perSeriesAligner" : "ALIGN_MEAN"
                        },
                        "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_release_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\" metric.label.\"EventCode\"=\"JobReleaseSuccess\""
                      },
                      "numerator" : {
                        "aggregation" : {
                          "alignmentPeriod" : "60s",
                          "crossSeriesReducer" : "REDUCE_SUM",
                          "groupByFields" : [],
                          "perSeriesAligner" : "ALIGN_MEAN"
                        },
                        "filter" : "metric.type=\"${google_monitoring_metric_descriptor.joblifecyclehelper_job_pulling_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\" metric.label.\"EventCode\"=\"JobPreparationSuccess\""
                      }
                    }
                  }
                }
              ],
              "thresholds" : [],
              "timeshiftDuration" : "0s",
              "yAxis" : {
                "label" : "",
                "scale" : "LINEAR"
              }
            }
          }
        ]
      }
    }
  )
}
