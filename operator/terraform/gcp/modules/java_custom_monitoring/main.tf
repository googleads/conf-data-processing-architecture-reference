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

resource "google_monitoring_metric_descriptor" "jobclient_job_validation_failure_metric" {
  display_name = "Job Client Validation Failures"
  description  = "Custom metric for validation failures in the job client."

  type        = "custom.googleapis.com/scp/jobclient/${var.environment}/jobvalidationfailure"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Validator"
  }
}

resource "google_monitoring_alert_policy" "jobclient_job_validation_failure_alert" {
  display_name = "${var.environment} Job Client Validation Failure Alert"
  combiner     = "OR"
  conditions {
    display_name = "Validation Failures"
    condition_threshold {
      filter     = "metric.type=\"${google_monitoring_metric_descriptor.jobclient_job_validation_failure_metric.type}\" AND resource.type=\"gce_instance\""
      duration   = "${var.alarm_duration_sec}s"
      comparison = "COMPARISON_GT"
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

resource "google_monitoring_metric_descriptor" "jobclient_job_client_error_metric" {
  display_name = "Job Client Errors"
  description  = "Custom metric for errors in the job client."

  type        = "custom.googleapis.com/scp/jobclient/${var.environment}/jobclienterror"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "ErrorReason"
  }
}

resource "google_monitoring_metric_descriptor" "worker_job_error_metric" {
  display_name = "Worker Job Errors"
  description  = "Custom metric for errors with worker job handling."

  type        = "custom.googleapis.com/scp/worker/${var.environment}/workerjoberror"
  metric_kind = "GAUGE"
  value_type  = "DOUBLE"

  labels {
    key = "Type"
  }
}

resource "google_monitoring_alert_policy" "worker_job_error_alert" {
  display_name = "${var.environment} Worker Job Errors Alert"
  combiner     = "OR"
  conditions {
    display_name = "Worker Job Errors"
    condition_threshold {
      filter     = "metric.type=\"${google_monitoring_metric_descriptor.worker_job_error_metric.type}\" AND resource.type=\"gce_instance\""
      duration   = "${var.alarm_duration_sec}s"
      comparison = "COMPARISON_GT"
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

resource "google_monitoring_dashboard" "worker_custom_metrics_dashboard" {
  dashboard_json = jsonencode(
    {
      "displayName" : "${var.environment} Worker Custom Metrics Dashboard",
      "gridLayout" : {
        "columns" : "2",
        "widgets" : [
          {
            "title" : "Worker and Job Client - Errors and Validation Failures",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "Job Client Errors",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "perSeriesAligner" : "ALIGN_SUM",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "metadata.system_labels.\"instance_group\""
                        ]
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.jobclient_job_client_error_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Job Client Validation Failures",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "perSeriesAligner" : "ALIGN_SUM",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "metadata.system_labels.\"instance_group\""
                        ]
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.jobclient_job_validation_failure_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Worker Job Errors",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "perSeriesAligner" : "ALIGN_SUM",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "metadata.system_labels.\"instance_group\""
                        ]
                      },
                      "filter" : "metric.type=\"${google_monitoring_metric_descriptor.worker_job_error_metric.type}\" resource.type=\"gce_instance\" metadata.system_labels.\"instance_group\"=\"${var.vm_instance_group_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                      }
                    }
                  }
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          }
        ]
      }
    }
  )
}
