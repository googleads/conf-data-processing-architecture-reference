/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.kms;

import org.testcontainers.containers.GenericContainer;
import org.testcontainers.utility.DockerImageName;
import org.testcontainers.utility.MountableFile;

public final class LocalKmsServerContainer extends GenericContainer<LocalKmsServerContainer> {
  private static final int invokerPort = 8000;

  private String serverFileName;
  private String serverJarPath;
  private String serverClassTarget;

  public LocalKmsServerContainer(
      DockerImageName dockerImageName,
      String serverFileName,
      String serverJarPath,
      String serverClassTarget) {
    super(dockerImageName);
    this.serverFileName = serverFileName;
    this.serverJarPath = serverJarPath;
    this.serverClassTarget = serverClassTarget;
    withExposedPorts(invokerPort);
    withCopyFileToContainer(MountableFile.forHostPath(getFunctionJarPath()), "/");
    setCommand(getContainerStartupCommand());
  }

  public String getEmulatorEndpoint() {
    return getHost() + ":" + getMappedPort(invokerPort);
  }

  private String getServerFilename() {
    return serverFileName;
  }

  private String getFunctionJarPath() {
    return serverJarPath + getServerFilename();
  }

  private String getServerClassTarget() {
    return serverClassTarget;
  }

  private String getContainerStartupCommand() {
    return "java -cp " + "./" + getServerFilename() + " " + getServerClassTarget();
  }

  public String getHttpPort() {
    return Integer.toString(invokerPort);
  }

  public static LocalKmsServerContainer startLocalKmsContainer(
      String serverFileName, String serverJarPath, String serverClassTarget) {
    LocalKmsServerContainer container =
        new LocalKmsServerContainer(
            DockerImageName.parse("openjdk:jdk-slim"),
            serverFileName,
            serverJarPath,
            serverClassTarget);
    container.start();
    return container;
  }
}
