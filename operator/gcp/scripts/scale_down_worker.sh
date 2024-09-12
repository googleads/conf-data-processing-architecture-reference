#!/usr/bin/env bash

# Copyright 2023 Google LLC
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

set -e

#######################################
# Scales worker instances down to zero.
# Should be run before terrform apply
# on the operator environment to
# prevent jobs from being interupted.
#
# Globals:
#   ENVIRONMENT
#   PROJECT
#   REGION
# Arguments:
#   None
# Returns:
#   0 if successful, non-zero on error.
#######################################
scale_down_worker() {
  local name
  local mig_prefix="${ENVIRONMENT}-worker-mig-"
  name=($(gcloud compute instance-groups managed list --project=$PROJECT --format=json | jq --raw-output --arg mig_prefix $mig_prefix ' .[].name | select( startswith($mig_prefix))'))
  if [[ -z ${name} ]]; then
    echo "No instances found for environment: $ENVIRONMENT"
  else
    # Set autoscaling to zero so that scp autoscaling waits for jobs to finish
    echo "Setting autoscaling to 0."
    gcloud compute instance-groups managed set-autoscaling ${name} --max-num-replicas=0 --min-num-replicas=0 --project=$PROJECT --region=$REGION
    local instances
    instances=($(gcloud compute instance-groups managed list-instances ${name} --project=$PROJECT --region=$REGION --format=json | jq .[]))
    while [[ ${#instances[@]} -gt 0 ]]; do
      sleep 15
      echo "Checking existing instances.."
      instances=($(gcloud compute instance-groups managed list-instances ${name} --project=$PROJECT --region=$REGION --format=json | jq .[]))
    done
    echo "All existing instances have been shutdown."
  fi
}

while [ $# -gt 0 ]; do
  case "$1" in
    --project=*)
      PROJECT="${1#*=}"
      ;;
    --region=*)
      REGION="${1#*=}"
      ;;
    --environment=*)
      ENVIRONMENT="${1#*=}"
      ;;
    *)
      printf "****************************\n"
      printf "* Error: Invalid argument. *\n"
      printf "****************************\n"
      exit 1
  esac
  shift
done

scale_down_worker
