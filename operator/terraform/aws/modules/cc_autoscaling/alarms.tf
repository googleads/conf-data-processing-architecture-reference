/**
 * Copyright 2023 Google LLC
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
# Auto Scaling Group Alarms
################################################################################

resource "aws_cloudwatch_metric_alarm" "asg_max_instances_alarm" {
  count               = var.enable_autoscaling && var.alarms_enabled ? 1 : 0
  alarm_name          = "${var.environment}_${var.region}_asg_max_instance_alarm"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 1
  threshold           = ceil(var.max_ec2_instances * var.asg_max_instances_alarm_ratio)
  metric_name         = "GroupDesiredCapacity"
  namespace           = "AWS/AutoScaling"
  period              = var.autoscaling_alarm_eval_period_sec
  statistic           = "Maximum"

  dimensions = {
    AutoScalingGroupName = aws_autoscaling_group.worker_group.name
  }
  alarm_actions = [var.sns_topic_arn]
}
