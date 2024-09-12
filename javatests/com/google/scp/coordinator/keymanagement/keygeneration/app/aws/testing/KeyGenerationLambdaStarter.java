/*
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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws.testing;

import static java.util.concurrent.TimeUnit.SECONDS;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Collections;
import java.util.Map;
import org.testcontainers.containers.Network;

/**
 * Utility class for invoking the key rotation lambda using the AWS SAM CLI.
 *
 * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
 */
@Deprecated
public final class KeyGenerationLambdaStarter {

  private static final String KEY_ROTATION_TEMPLATE_YAML =
      "javatests/com/google/scp/coordinator/keymanagement/keygeneration/app/aws/testing/key_rotation_generated.yaml";

  private KeyGenerationLambdaStarter() {}

  /** Returns the path to a temporary JSON file that contains the environment variables. */
  private static String createKeyRotationJson(Map<String, String> environmentVariables)
      throws IOException {
    var file = File.createTempFile("key_rotation", ".json");
    file.deleteOnExit();

    ObjectMapper mapper = new ObjectMapper();
    ObjectNode params = mapper.createObjectNode();
    ObjectNode vars = mapper.createObjectNode();

    environmentVariables.entrySet().stream().forEach(e -> vars.put(e.getKey(), e.getValue()));
    params.set("Parameters", vars);

    String json = mapper.writerWithDefaultPrettyPrinter().writeValueAsString(params);
    Files.write(file.toPath(), Collections.singleton(json), StandardCharsets.UTF_8);

    return file.getPath();
  }

  /** Starts the key generation lambda using SAM with the provided environment variables. */
  public static Process runKeyRotationLambda(
      Map<String, String> environmentVariables, Network network)
      throws IOException, InterruptedException {
    var pb = new ProcessBuilder();
    pb.command(
            "sam",
            "local",
            "invoke",
            // Mount local path as a docker volume, otherwise some weird behavior can happen when
            // trying to resolve the jar file relative to the symlinked yaml file.
            "-v",
            ".",
            "--env-vars",
            createKeyRotationJson(environmentVariables),
            "--docker-network",
            network.getId(),
            "-t",
            KEY_ROTATION_TEMPLATE_YAML)
        .inheritIO();
    var process = pb.start();
    // unlike start-api, invoke runs the lambda once and exits when the run is complete. Block the
    // thread until the process has completed.
    process.waitFor(30, SECONDS);
    return process;
  }
}
