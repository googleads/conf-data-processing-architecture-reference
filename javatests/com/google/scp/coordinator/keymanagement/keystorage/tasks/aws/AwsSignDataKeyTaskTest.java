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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.aws;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.security.GeneralSecurityException;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.ZoneId;
import java.util.Base64;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import software.amazon.awssdk.services.kms.model.VerifyResponse;

@RunWith(JUnit4.class)
public final class AwsSignDataKeyTaskTest {
  private static final Instant NOW = Instant.ofEpochSecond(1074560000);
  private static final String signatureKeyUri = "aws:arn:...";
  private static final String VALID_CONTEXT = NOW.toString();
  private static final String signatureAlgorithm = "ECDSA_SHA_256";

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Mock private PublicKeySign publicKeySign;
  @Mock private PublicKeyVerify publicKeyVerify;

  private Clock clock = Clock.fixed(NOW, ZoneId.of("UTC"));
  private final VerifyResponse validVerifyResponse =
      VerifyResponse.builder().signatureValid(true).build();

  private AwsSignDataKeyTask task;

  @Before
  public void setUp() {
    task = new AwsSignDataKeyTask(clock, publicKeySign, publicKeyVerify);
  }

  @Test
  public void signDataKey_success() throws Exception {
    when(publicKeySign.sign(any(byte[].class))).thenReturn("signaturez".getBytes());
    var expectedSignature = new String(Base64.getEncoder().encode("signaturez".getBytes()));
    var arg = ArgumentCaptor.forClass(byte[].class);
    var dataKey =
        DataKey.newBuilder().setEncryptedDataKey("beep").setEncryptedDataKeyKekUri("boop").build();

    var signedKey = task.signDataKey(dataKey);

    assertThat(signedKey.getDataKeyContext()).isEqualTo("2004-01-20T00:53:20Z");
    assertThat(signedKey.getMessageAuthenticationCode()).isEqualTo(expectedSignature);
    verify(publicKeySign, times(1)).sign(arg.capture());
    // context%%encryptedKey%%uri
    assertThat(arg.getValue()).isEqualTo("2004-01-20T00:53:20Z%%beep%%boop".getBytes());
  }

  @Test
  public void signDataKey_throwsIfMissingFields() throws Exception {
    when(publicKeySign.sign(any(byte[].class))).thenReturn("signaturez".getBytes());
    var noKekUri = DataKey.newBuilder().setEncryptedDataKey("beep").build();
    var noEncryptedDataKey = DataKey.newBuilder().setEncryptedDataKeyKekUri("boop").build();

    var ex1 = assertThrows(IllegalArgumentException.class, () -> task.signDataKey(noKekUri));
    var ex2 =
        assertThrows(IllegalArgumentException.class, () -> task.signDataKey(noEncryptedDataKey));

    assertThat(ex1).hasMessageThat().contains("missing");
    assertThat(ex2).hasMessageThat().contains("missing");
    verify(publicKeySign, times(0)).sign(any(byte[].class));
  }

  @Test
  public void signDataKey_throwsIfSigned() throws Exception {
    when(publicKeySign.sign(any(byte[].class))).thenReturn("signaturez".getBytes());
    var signedKey =
        DataKey.newBuilder()
            .setEncryptedDataKey("beep")
            .setEncryptedDataKeyKekUri("boop")
            .setDataKeyContext("bar")
            .setMessageAuthenticationCode("baz")
            .build();

    var response = assertThrows(IllegalArgumentException.class, () -> task.signDataKey(signedKey));
    assertThat(response).hasMessageThat().contains("contains signature");
    verify(publicKeySign, times(0)).sign(any(byte[].class));
  }

  @Test
  public void signDataKey_throwsOnKmsError() throws Exception {
    when(publicKeySign.sign(any(byte[].class))).thenThrow(new GeneralSecurityException("eep"));
    var dataKey =
        DataKey.newBuilder().setEncryptedDataKey("beep").setEncryptedDataKeyKekUri("boop").build();

    var response = assertThrows(ServiceException.class, () -> task.signDataKey(dataKey));

    assertThat(response.getCause()).hasMessageThat().contains("eep");
  }

  @Test
  public void verifyDataKey_success() throws Exception {
    doNothing().when(publicKeyVerify).verify(any(byte[].class), any(byte[].class));
    var argSignature = ArgumentCaptor.forClass(byte[].class);
    var argMessage = ArgumentCaptor.forClass(byte[].class);
    var signedKey =
        DataKey.newBuilder()
            .setEncryptedDataKey("beep")
            .setEncryptedDataKeyKekUri("boop")
            .setDataKeyContext(VALID_CONTEXT)
            .setMessageAuthenticationCode("baz")
            .build();

    task.verifyDataKey(signedKey);

    verify(publicKeyVerify, times(1)).verify(argSignature.capture(), argMessage.capture());
    assertThat(argSignature.getValue()).isEqualTo(Base64.getDecoder().decode("baz".getBytes()));
    // context%%encryptedKey%%uri
    assertThat(argMessage.getValue()).isEqualTo((VALID_CONTEXT + "%%beep%%boop").getBytes());
  }

  @Test
  public void verifyDataKey_failsIfUnsigned() throws Exception {
    doNothing().when(publicKeyVerify).verify(any(byte[].class), any(byte[].class));
    var unsignedKey =
        DataKey.newBuilder().setEncryptedDataKey("beep").setEncryptedDataKeyKekUri("boop").build();

    var ex = assertThrows(ServiceException.class, () -> task.verifyDataKey(unsignedKey));

    assertThat(ex.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    verify(publicKeyVerify, times(0)).verify(any(byte[].class), any(byte[].class));
  }

  @Test
  public void verifyDataKey_failsIfKmsRequestFails() throws Exception {
    doThrow(new GeneralSecurityException("eep"))
        .when(publicKeyVerify)
        .verify(any(byte[].class), any(byte[].class));
    var signedKey =
        DataKey.newBuilder()
            .setEncryptedDataKey("beep")
            .setEncryptedDataKeyKekUri("boop")
            .setDataKeyContext(VALID_CONTEXT)
            .setMessageAuthenticationCode("baz")
            .build();

    var ex = assertThrows(ServiceException.class, () -> task.verifyDataKey(signedKey));

    assertThat(ex).hasMessageThat().contains("Signature validation failed");
    verify(publicKeyVerify, times(1)).verify(any(byte[].class), any(byte[].class));
  }

  @Test
  public void verifyDataKey_failsIfExpiredKey() throws Exception {
    doNothing().when(publicKeyVerify).verify(any(byte[].class), any(byte[].class));
    var oldKey =
        DataKey.newBuilder()
            .setEncryptedDataKey("beep")
            .setEncryptedDataKeyKekUri("boop")
            .setDataKeyContext(NOW.minus(Duration.ofDays(1)).toString())
            .setMessageAuthenticationCode("baz")
            .build();
    var futureKey =
        DataKey.newBuilder()
            .setEncryptedDataKey("beep")
            .setEncryptedDataKeyKekUri("boop")
            .setDataKeyContext(NOW.plus(Duration.ofDays(1)).toString())
            .setMessageAuthenticationCode("baz")
            .build();

    var ex1 = assertThrows(ServiceException.class, () -> task.verifyDataKey(oldKey));
    var ex2 = assertThrows(ServiceException.class, () -> task.verifyDataKey(futureKey));

    assertThat(ex1.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    assertThat(ex2.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    assertThat(ex1).hasMessageThat().contains("The provided data key is expired.");
    assertThat(ex2).hasMessageThat().contains("The provided data key is expired.");
  }

  @Test
  public void verifyDataKey_failsIfFutureKey() throws Exception {
    doNothing().when(publicKeyVerify).verify(any(byte[].class), any(byte[].class));
    var signedKey =
        DataKey.newBuilder()
            .setEncryptedDataKey("beep")
            .setEncryptedDataKeyKekUri("boop")
            .setDataKeyContext(NOW.plus(Duration.ofDays(1)).toString())
            .setMessageAuthenticationCode("baz")
            .build();

    var ex = assertThrows(ServiceException.class, () -> task.verifyDataKey(signedKey));

    assertThat(ex.getErrorCode()).isEqualTo(Code.INVALID_ARGUMENT);
    assertThat(ex).hasMessageThat().contains("The provided data key is expired.");
  }
}
