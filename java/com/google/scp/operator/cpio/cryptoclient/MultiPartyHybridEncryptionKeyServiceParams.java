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

package com.google.scp.operator.cpio.cryptoclient;

import com.google.auto.value.AutoValue;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;

// Parameters to construct a MultiPartyHybridEncryptionKeyService class
@AutoValue
public abstract class MultiPartyHybridEncryptionKeyServiceParams {

  /** Returns a new builder. */
  public static Builder builder() {
    return new AutoValue_MultiPartyHybridEncryptionKeyServiceParams.Builder();
  }

  /** Returns the {@link EncryptionKeyFetchingService} for coordinator A. */
  public abstract EncryptionKeyFetchingService coordAKeyFetchingService();

  /** Returns the {@link EncryptionKeyFetchingService} for coordinator B. */
  public abstract EncryptionKeyFetchingService coordBKeyFetchingService();

  /** Returns the {@link CloudAeadSelector} for coordinator A. */
  public abstract CloudAeadSelector coordAAeadService();

  /** Returns the {@link CloudAeadSelector} for coordinator B. */
  public abstract CloudAeadSelector coordBAeadService();

  /** Builder for {@link MultiPartyHybridEncryptionKeyServiceParams}. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Create a new {@link MultiPartyHybridEncryptionKeyServiceParams} from the builder. */
    public abstract MultiPartyHybridEncryptionKeyServiceParams build();

    /** Sets coordinator A {@link EncryptionKeyFetchingService} object. */
    public abstract Builder setCoordAKeyFetchingService(
        EncryptionKeyFetchingService coordAKeyFetchingService);

    /** Sets coordinator B {@link EncryptionKeyFetchingService} object. */
    public abstract Builder setCoordBKeyFetchingService(
        EncryptionKeyFetchingService coordBKeyFetchingService);

    /** Sets coordinator A {@link CloudAeadSelector} object. */
    public abstract Builder setCoordAAeadService(CloudAeadSelector coordAAeadService);

    /** Sets coordinator B {@link CloudAeadSelector} object. */
    public abstract Builder setCoordBAeadService(CloudAeadSelector coordBAeadService);
  }
}
