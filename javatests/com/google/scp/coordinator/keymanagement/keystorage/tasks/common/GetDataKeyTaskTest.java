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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.common;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.crypto.tink.Aead;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.keymanagement.testutils.FakeDataKeyUtil;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.util.Base64Util;
import com.google.scp.shared.util.EncryptionUtil;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class GetDataKeyTaskTest {

  private static final String COORDINATOR_KEK_URI = "http://uri";
  private static final ByteString ENCRYPTED_KEY = ByteString.copyFrom("encrypted_key".getBytes());
  private static final DataKey signedDataKey = FakeDataKeyUtil.createDataKey();
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock Aead aead;
  @Mock EncryptionUtil encryptionUtility;
  @Mock SignDataKeyTask signDataKeyTask;

  private GetDataKeyTask task;

  @Before
  public void setUp() throws Exception {
    task = new GetDataKeyTask(encryptionUtility, signDataKeyTask, aead, COORDINATOR_KEK_URI);
    when(encryptionUtility.generateSymmetricEncryptedKey(aead)).thenReturn(ENCRYPTED_KEY);
    when(signDataKeyTask.signDataKey(any(DataKey.class))).thenReturn(signedDataKey);
  }

  @Test
  public void getDataKey_Returns_DataKey() throws ServiceException {
    var unsignedDataKey = ArgumentCaptor.forClass(DataKey.class);

    var returnedDataKey = task.getDataKey();

    assertThat(returnedDataKey).isEqualTo(signedDataKey);

    verify(signDataKeyTask, times(1)).signDataKey(unsignedDataKey.capture());
    assertThat(unsignedDataKey.getValue().getEncryptedDataKeyKekUri())
        .isEqualTo(COORDINATOR_KEK_URI);
    assertThat(unsignedDataKey.getValue().getEncryptedDataKey())
        .isEqualTo(Base64Util.toBase64String(ENCRYPTED_KEY));
  }
}
