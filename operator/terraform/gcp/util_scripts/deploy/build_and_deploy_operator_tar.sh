#!/usr/bin/env bash

# Copyright 2022 Google LLC
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

set -eu

# Prints usage instructions
print_usage() {
  echo "Usage:  ./operator/terraform/gcp/util_scripts/deploy/build_and_deploy_operator_tar.sh --environment=<environment> --environment_group=<environment_group> --terraform_path=<terraform_path> --tar_name=<tar_name> [--worker_image=<worker_image>] [--action=<action>] [--auto_approve=<true/false>]"
  printf "\n\t--environment_path        - The path of the operator environment to deploy.\n"
  printf "\t--original_terraform_path   - The original path of the terraform file to deploy.\n"
  printf "\t--tar_name                  - The target name of the operator tar.\n"
  printf "\t--worker_image              - Artifact registry worker image. Default uses tfvars variable.\n"
  printf "\t--action                    - Terraform action to take (apply|destroy). Default is apply.\n"
  printf "\t--auto_approve              - Whether to auto-approve the Terraform action. Default is false.\n"
}

setup() {
    # Clear and recreate environment path
    rm -rf "$ENV_PATH"
    mkdir -p "$ENV_PATH"

    # build tar
    bazel build //operator/terraform/gcp:$TAR_NAME

    # extract tar to environment path
    TAR_FILE="$(bazel info bazel-bin)/operator/terraform/gcp/$TAR_NAME.tgz"
    tar xzf "$TAR_FILE" -C "$ENV_PATH/../.."

    # copy terraform files from original to environment path
    cp -r -T "$ORIGINAL_TERRAFORM_PATH" "$ENV_PATH"

    # create java folder for storing jar files later on
    mkdir -p "$ENV_PATH/../../java"
}

if [ $# -eq 0 ]
  then
    printf "ERROR: Operator environment (ENV) must be provided.\n"
    exit 1
fi

FUNCTION=$1
# parse arguments
for ARGUMENT in "$@"
  do
    case $ARGUMENT in
      --worker_image=*)
        WORKER_IMAGE=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --environment_path=*)
        ENV_PATH=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --original_terraform_path=*)
        ORIGINAL_TERRAFORM_PATH=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --tar_name=*)
        TAR_NAME=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --action=*)
        input=$(echo "$ARGUMENT" | cut -f2 -d=)
        if [[ $input = "apply" || $input = "destroy" || $input = "plan" ]]
        then
          ACTION=$input
        else
          printf "ERROR: Input action must be one of (apply|destroy|plan).\n"
          exit 1
        fi
        ;;
      --auto_approve=*)
        AUTO_APPROVE=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      help)
        print_usage
        exit 1
        ;;
      *)
        printf "ERROR: invalid argument $ARGUMENT\n"
        print_usage
        exit 1
        ;;
    esac
done

if [[ -z ${ENV_PATH+x} ]]; then
  printf "ERROR: Operator environment (ENV_PATH) must be provided.\n"
  exit 1
fi

# Build and extract operator tar and environment.
setup

PARAMETERS=''

if [[ -n ${WORKER_IMAGE+x} ]]; then
    PARAMETERS="${PARAMETERS} --worker_image=$WORKER_IMAGE "
fi

if [[ -n ${ACTION+x} ]]; then
    PARAMETERS="${PARAMETERS} --action=$ACTION "
fi

if [[ -n ${AUTO_APPROVE+x} ]]; then
    PARAMETERS="${PARAMETERS} --auto_approve=$AUTO_APPROVE "
fi

# Deploy operator environment with specified action.
$(bazel info workspace)/operator/terraform/gcp/util_scripts/deploy/operator_deploy.sh --environment_path=$ENV_PATH $PARAMETERS


