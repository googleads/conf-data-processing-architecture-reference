/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.KeyIdFactory;

import static com.google.common.truth.Truth.assertThat;

import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.SequenceKeyIdFactory;
import com.google.scp.coordinator.keymanagement.shared.dao.testing.InMemoryKeyDb;
import com.google.scp.coordinator.keymanagement.testutils.FakeEncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Test SequenceKeyIdFactory. */
@RunWith(JUnit4.class)
public class SequenceKeyIdFactoryTest {

  private final SequenceKeyIdFactory keyIdFactory = new SequenceKeyIdFactory();
  private final InMemoryKeyDb keyDb = new InMemoryKeyDb();

  @Test
  public void testEncodeAndDecode() {
    assertThat(keyIdFactory.decodeKeyIdFromString(keyIdFactory.encodeKeyIdToString(0L)))
        .isEqualTo(0L);
    assertThat(keyIdFactory.decodeKeyIdFromString(keyIdFactory.encodeKeyIdToString(1L)))
        .isEqualTo(1L);
    assertThat(keyIdFactory.decodeKeyIdFromString(keyIdFactory.encodeKeyIdToString(16L)))
        .isEqualTo(16L);
    assertThat(keyIdFactory.decodeKeyIdFromString(keyIdFactory.encodeKeyIdToString(Long.MAX_VALUE)))
        .isEqualTo(Long.MAX_VALUE);
  }

  @Test
  public void testGetNextKeyId_withOverflow() throws ServiceException {
    keyDb.createKey(FakeEncryptionKey.withKeyId(keyIdFactory.encodeKeyIdToString(Long.MAX_VALUE)));
    String nextKeyId = keyIdFactory.getNextKeyId(keyDb);
    assertThat(nextKeyId).isEqualTo(keyIdFactory.encodeKeyIdToString(Long.MIN_VALUE));
  }

  @Test
  public void testGetNextKeyId_afterOverflow() throws ServiceException {
    keyDb.createKey(FakeEncryptionKey.withKeyId(keyIdFactory.encodeKeyIdToString(Long.MAX_VALUE)));
    keyDb.createKey(FakeEncryptionKey.withKeyId(keyIdFactory.encodeKeyIdToString(Long.MIN_VALUE)));
    String nextKeyId = keyIdFactory.getNextKeyId(keyDb);
    assertThat(nextKeyId).isEqualTo(keyIdFactory.encodeKeyIdToString(Long.MIN_VALUE + 1));
  }

  @Test
  public void testGetNextKeyId_empty() throws ServiceException {
    String nextKeyId = keyIdFactory.getNextKeyId(keyDb);
    assertThat(nextKeyId).isEqualTo(keyIdFactory.encodeKeyIdToString(0L));
  }

  @Test
  public void testGetNextKeyId() throws ServiceException {
    int keysToCreate = 100;
    for (int i = 0; i < keysToCreate; i++) {
      String nextKeyId = keyIdFactory.getNextKeyId(keyDb);
      assertThat(nextKeyId).isEqualTo(keyIdFactory.encodeKeyIdToString((long) i));
      keyDb.createKey(FakeEncryptionKey.withKeyId(nextKeyId));
    }
  }
}
