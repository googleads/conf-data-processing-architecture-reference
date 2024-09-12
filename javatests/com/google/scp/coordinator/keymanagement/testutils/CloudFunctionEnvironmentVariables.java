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

package com.google.scp.coordinator.keymanagement.testutils;

/** Environment variable names. */
public class CloudFunctionEnvironmentVariables {

  public static final String ENV_VAR_GCP_PROJECT_ID = "GCP_PROJECT_ID";
  public static final String ENV_VAR_SPANNER_INSTANCE_ID = "SPANNER_INSTANCE_ID";
  public static final String ENV_VAR_SPANNER_DB_NAME = "SPANNER_DB_NAME";
  public static final String ENV_VAR_SPANNER_ENDPOINT = "SPANNER_EMULATOR_HOST";
}
