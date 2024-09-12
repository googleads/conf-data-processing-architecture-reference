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

package com.google.scp.operator.frontend.tasks.validation;

import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;
import java.util.Optional;

/** Performs validation on requests to create jobs. */
public interface RequestInfoValidator {

  /**
   * Performs some validation on the {@code RequestInfo}. Optional will be empty if the validation
   * succeeds or will contain an error message validation fails.
   */
  Optional<String> validate(RequestInfo requestInfo);
}
