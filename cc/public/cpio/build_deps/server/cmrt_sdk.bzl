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

load("//cc/public/cpio/build_deps/shared:build_sdk_runtime_image.bzl", "build_sdk_runtime_image")

# Bring up all SDK servers.
def cmrt_sdk(
        *,
        name,
        platform = "aws",
        inside_tee = True,
        is_test_server = False,
        recover_client_binaries = True,
        recover_sdk_binaries = True,
        client_binaries = {},
        auto_scaling_service_configs = {},
        job_service_configs = {},
        nosql_database_service_configs = {},
        private_key_service_configs = {},
        public_key_service_configs = {},
        queue_service_configs = {},
        additional_env_variables = {},
        additional_files = [],
        additional_tars = [],
        ports = [],
        sdk_cmd_override = []):
    sdk_servers = {
        "auto_scaling_service": auto_scaling_service_configs,
        "blob_storage_service": {},
        "crypto_service": {},
        "instance_service": {},
        "job_service": job_service_configs,
        "metric_service": {},
        "nosql_database_service": nosql_database_service_configs,
        "parameter_service": {},
        "private_key_service": private_key_service_configs,
        "public_key_service": public_key_service_configs,
        "queue_service": queue_service_configs,
    }

    build_sdk_runtime_image(
        name = name,
        client_binaries = client_binaries,
        additional_env_variables = additional_env_variables,
        inside_tee = inside_tee,
        is_test_server = is_test_server,
        platform = platform,
        recover_client_binaries = recover_client_binaries,
        recover_sdk_binaries = recover_sdk_binaries,
        additional_files = additional_files,
        additional_tars = additional_tars,
        sdk_cmd_override = sdk_cmd_override,
        ports = ports,
        sdk_binaries = sdk_servers,
    )
