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

package com.google.scp.coordinator.keymanagement.testutils.aws;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Test-only annotations for representing entities in test-only environments e.g. for disambiguating
 * URIs when a test environment simulates 2 coordinators.
 */
public final class Annotations {
  private Annotations() {}

  /**
   * URI representing the KMS URI used to encrypt keys by Coordinator A before sharing them with a
   * worker process.
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorAWorkerKekUri {}

  /** See {@link CoordinatorAWorkerKekUri} */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorBWorkerKekUri {}

  /** Table name for the Key DB run by Coordinator A. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorAKeyDbTableName {}

  /** Table name for the Key DB run by Coordinator B. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorBKeyDbTableName {}

  /** Annotation for representing the base URL of public key vending service. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface ListPublicKeysBaseUrl {}

  /** Annotation for the key id used to sign the public key material */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorAEncryptionKeySignatureKeyId {}

  /** Annotation for the algorithm used by {@link PublicKeySignatureKeyId} */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorAEncryptionKeySignatureAlgorithm {}
}
