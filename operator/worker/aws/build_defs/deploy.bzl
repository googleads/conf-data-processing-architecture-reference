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

load("//build_defs/aws/enclave:container.bzl", "java_enclave_image")
load("//operator/worker/aws/build_defs:ami.bzl", "packer_worker_ami")

def worker_aws_deployment(
        *,
        name,
        repo_name = "",
        ami_name = ":ami_name_flag",
        ami_groups = "[]",
        packer_ami_config = "//operator/worker/aws:aggregation_worker_ami.pkr.hcl",
        ec2_instance = "m5.xlarge",
        subnet_id = ":subnet_id_flag",
        enclave_cpus = 2,
        enclave_memory_mib = 7168,
        aws_region = ":aws_region_flag",
        test_key = None,
        test_avro_file = None,
        enclave_env_variables = {},
        enclave_cmd = [],
        worker_path = "//java/com/google/scp/operator/worker:WorkerRunnerDeploy",
        proxy_rpm = "//cc/proxy:vsockproxy_rpm_al2023",
        worker_watcher_rpm = "//operator/worker/aws/enclave:aggregate_worker_rpm",
        jar_file = None,
        jar_args = [],
        jvm_options = [],
        licenses = "//licenses:licenses_tar",
        enable_worker_debug_mode = False,
        uninstall_ssh_server = True,
        user_rpms = []):
    """Creates targets for AWS AMI generation and enclave Docker container.

    Assuming the name passed is 'worker', this macro instantiates multiple
    targets:
      1. calls container_image macro with name 'worker_container': check the
         documentation for container_image macro for details what targets are
         available; notable targets are 'worker_container' which represent the
         layer for the Docker image, as well as 'worker_container.tar' which is
         the complete tar archive containing the full Docker image
      2. 'worker': executable target which downloads Packer and runs it to
         prepare the AMI image containing the proxy, proxy configuration and is
         ready to run the enclave upon startup.

    Args:
      name: The target name used to generate the targets described above.
      repo_name: Remote bazel repository to source Secure Control Plane artifacts
        from (e.g. "@google_repo_tee") or "" to reference the local repository.
        Must be provided when using this build rule from another repository and
        should match the name of the remote repository used to import this build rule.
      ami_name: Path to a `string_flag` target defining the name of the AMI that
        is generated by the target.
      ami_groups: Groups to share AMI with. Default is none. Change to ["all"]
        to make AMI public.
      packer_ami_config: Path to the packer config template.
      ec2_instance: Ec2 instance type for running packer script.
      subnet_id: Path to a `string_flag` target defining the subnet in which to
        deploy the EC2 instance to execute the packer scripts.
      enclave_cpus: Number of cores assigned to enclave.
      enclave_memory_mib: Amount of memory (in MB) assigned to enclave.
      aws_region: Path to a `string_flag` target defining the AWS region to run the
        packer script in.
      test_key: Bazel label of the key to be added to the enclave.
      test_avro_file: Bazel label of the test Avro file to be added to the
        enclave.
      enclave_env_variables: Dict (string-to-string) of environment variables to
        be added to the enclave.
      enclave_cmd: Command (in exec form; please consult Docker documentation
        for what this exactly is) to start up the enclave container. Only used
        for testing purposes. Please leave empty otherwise.
      worker_path: Path to worker deployable jar.
      proxy_rpm: Path to proxy rpm.
      worker_watcher_rpm: Path to worker watcher rpm.
      jar_file: Absolute path to the JAR file in the enclave that should be
        invoked.
      jar_args: CLI args passed to the JAR file inside the enclave.
      jvm_options: Jvm options passed to control JVM inside the enclave.
      licenses: This should be a label of a tar file containing all the licenses
        of our distribution and all dependencies.
      enable_worker_debug_mode: Whether to run enclave in debug mode.
      uninstall_ssh_server: Whether to uninstall SSH server from AMI once
        provisioning is completed (SSH is used for provisioning).
    """

    additional_container_files = []

    if test_key:
        additional_container_files.append(test_key)

    if test_avro_file:
        additional_container_files.append(test_avro_file)

    docker_target_name = "%s_container" % name

    # Build target for the Docker container
    java_enclave_image(
        name = docker_target_name,
        jar_target = worker_path,
        jar_filename = jar_file,
        jar_args = jar_args,
        jvm_options = jvm_options,
        enclave_cmd_override = enclave_cmd,
        enclave_env_variables = enclave_env_variables,
        repo_name = repo_name,
        additional_files = additional_container_files,
    )

    # Executable target for running Packer to generate the complete AMI image
    packer_worker_ami(
        name = name,
        enclave_container_image = ":%s.tar" % docker_target_name,
        proxy_rpm = "%s%s" % (repo_name, proxy_rpm),
        worker_watcher_rpm = "%s%s" % (repo_name, worker_watcher_rpm),
        packer_ami_config = "%s%s" % (repo_name, packer_ami_config),
        ami_name = ami_name,
        ami_groups = ami_groups,
        ec2_instance = ec2_instance,
        subnet_id = subnet_id,
        enclave_cpus = enclave_cpus,
        enclave_memory_mib = enclave_memory_mib,
        aws_region = aws_region,
        enable_worker_debug_mode = enable_worker_debug_mode,
        uninstall_ssh_server = uninstall_ssh_server,
        licenses = licenses,
        user_rpms = user_rpms,
    )
