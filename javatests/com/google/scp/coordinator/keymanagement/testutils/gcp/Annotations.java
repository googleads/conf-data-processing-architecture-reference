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

package com.google.scp.coordinator.keymanagement.testutils.gcp;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Test-only annotations for representing entities in test-only environments. */
public final class Annotations {
  private Annotations() {}

  /** Container for Public Key Cloud Function Integration tests */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface PublicKeyCloudFunctionContainer {}

  /** Container for Private Key Cloud Function Integration tests */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface PrivateKeyCloudFunctionContainer {}

  /** Container for Encryption Key Cloud Function Integration tests */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface EncryptionKeyServiceCloudFunctionContainer {}

  /** Note: @EncryptionKeyServiceCloudFunctionContainer above is for Coordinator A */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface EncryptionKeyServiceCoordinatorBCloudFunctionContainer {}

  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface KeyStorageCloudFunctionContainer {}

  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface KeyStorageEnvironmentVariables {}

  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface EncryptionKeyCoordinatorAEnvironmentVariables {}

  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface EncryptionKeyCoordinatorBEnvironmentVariables {}

  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface LocalKmsEnvironmentVariables {}

  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface TestLocalKmsServerContainer {}

  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface KeyStorageCloudFunctionContainerWithKms {}
}
