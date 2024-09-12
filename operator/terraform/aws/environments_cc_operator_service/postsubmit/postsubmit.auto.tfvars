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

# Example values required by operator_service.tf
#
# These values should be modified for each of your environments.

region      = "us-east-1"
environment = "cc-postsubmit"

ami_name   = "cc-postsubmit-aws-cmrt-worker" # from //java/com/google/scp/operator/cmrtworker/deploy/aws/BUILD
ami_owners = ["self"]

# Total resources available affected by instance_type -- actual resources used
# is affected by enclave_cpu_count / enclave_memory_mib. All 3 values should be
# updated at the same time.
instance_type      = "m5.2xlarge" # 8 cores, 32GiB
enclave_cpu_count  = 6            # Leave 2 vCPUs to host OS (minimum required).
enclave_memory_mib = 28672        # 28 GiB. ~30GiB are available on m5.2xlarge, leave 2GiB for host OS.

coordinator_a_assume_role_parameter = "arn:aws:iam::221820322062:role/mp-primary-pse2e_221820322062_coordinator_assume_role"
coordinator_b_assume_role_parameter = "arn:aws:iam::221820322062:role/mp-secondary-pse2e_221820322062_coordinator_assume_role"

initial_capacity_ec2_instances = 1
min_capacity_ec2_instances     = "1"
max_capacity_ec2_instances     = "2"

alarm_notification_email = "fakeemail@google.com"
