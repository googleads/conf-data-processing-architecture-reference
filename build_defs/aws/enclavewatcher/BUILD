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

load("@com_github_google_rpmpack//:def.bzl", "pkg_tar2rpm")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(
    default_visibility = ["//visibility:public"],
)

pkg_tar(
    name = "watcher_script",
    srcs = [
        "enclave_watcher.sh",
        "start_debug_enclave.sh",
    ],
    mode = "0755",
    ownername = "root.root",
    package_dir = "opt/google/scp",
)

pkg_tar(
    name = "watcher_service",
    srcs = [
        # LINT.IfChange
        "enclave-watcher.service",
        # LINT.ThenChange(:start_debug_enclave.sh)
    ],
    mode = "0644",
    ownername = "root.root",
    package_dir = "etc/systemd/system",
)

pkg_tar(
    name = "enclave_watcher_tar",
    ownername = "root.root",
    deps = [
        ":watcher_script",
        ":watcher_service",
    ],
)

pkg_tar2rpm(
    name = "enclave_watcher_rpm",
    data = ":enclave_watcher_tar",
    pkg_name = "enclave_watcher",
    # Signal to systemd that the service files changed on install.
    postin = """
    systemctl daemon-reload
    """,
    # Signal to systemd that the service files changed on uninstall.
    postun = """
    systemctl daemon-reload
    """,
    # Stop service before uninstall starts.
    preun = """
    systemctl stop enclave-watcher.service
    systemctl disable enclave-watcher.service
    """,
    release = "0",
    requires = [
        "python3-pyyaml",
    ],
    use_dir_allowlist = True,
    version = "0.0.1",
)
