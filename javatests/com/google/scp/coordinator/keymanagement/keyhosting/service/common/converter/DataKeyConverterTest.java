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

package com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter;

import static com.google.common.truth.Truth.assertThat;

import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.DataKeyProto;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class DataKeyConverterTest {
  @Test
  public void convert_doesRoundTrip() {
    var converter = new DataKeyConverter();

    var dataKey =
        DataKey.newBuilder()
            .setEncryptedDataKey("foo")
            .setEncryptedDataKeyKekUri("bar")
            .setDataKeyContext("baz")
            .setMessageAuthenticationCode("mac")
            .build();

    assertThat(converter.reverse().convert(converter.convert(dataKey))).isEqualTo(dataKey);
  }

  @Test
  public void convert_doesReverseRoundTrip() {
    var converter = new DataKeyConverter();

    var dataKey =
        DataKeyProto.DataKey.newBuilder()
            .setEncryptedDataKey("foo")
            .setEncryptedDataKeyKekUri("bar")
            .setDataKeyContext("baz")
            .setMessageAuthenticationCode("mac")
            .build();

    assertThat(converter.convert(converter.reverse().convert(dataKey))).isEqualTo(dataKey);
  }
}
