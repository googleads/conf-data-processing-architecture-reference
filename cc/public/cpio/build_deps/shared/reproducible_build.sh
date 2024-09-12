#!/bin/bash
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

#######################################
# Setup the container that will be used to run the build,
#######################################
setup_build_time_container() {
  local path_to_build_time_image_target="$1"
  local path_to_build_time_image="$2"

  bazel build $path_to_build_time_image_target
  if ! [ -f "$path_to_build_time_image" ]; then
    echo "image [$path_to_build_time_image] does not exist!"
    exit
  fi
  docker load < $path_to_build_time_image
  return $?
}

#######################################
# Run the given command within the build time container,
# using the given directory as the working directory.
# Make sure the container allows running the command as if it
# were running on the host, by using the host's resources and
# executing everything as the user running this function on the host.
#######################################
run_within_build_time_container() {
  local image="$1"
  local startup_directory="$2"
  local command_to_run="$3"
  # Support to run git inside docker by making running git commands
  # without git user and email. This will change machine's ~/.gitconfig.
  # If run locally, need sudo to gain permission, and after the run,
  # need to manually change the ~/.gitconfig back.
  git config --global --add safe.directory $startup_directory
  docker run -i \
      --privileged \
      --network host \
      --ipc host \
      -e HOME=$HOME \
      -v ~/.gitconfig:/etc/gitconfig:ro \
      -v "$HOME:$HOME" \
      -v "/tmpfs:/tmpfs" \
      -v /var/run/docker.sock:/var/run/docker.sock \
      -v "$startup_directory:$startup_directory" \
      -w "$startup_directory" \
      $image \
      "$command_to_run"
  return $?
}
