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

package com.google.scp.operator.cpio.blobstorageclient.gcp;

import static com.google.common.truth.Truth.assertThat;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.cloud.storage.BucketInfo;
import com.google.cloud.storage.Storage;
import com.google.common.hash.HashCode;
import com.google.common.hash.HashFunction;
import com.google.common.hash.Hashing;
import com.google.common.io.ByteStreams;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.BlobStorageClientException;
import com.google.scp.operator.cpio.blobstorageclient.ErrorReason;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import com.google.scp.shared.testutils.gcp.LocalGcsContainerTestModule;
import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class GcsBlobStorageClientIntegrationTest {

  @Rule public Acai acai = new Acai(TestEnv.class);

  @Rule public TemporaryFolder folder = new TemporaryFolder();

  @Inject private Storage gcsStorage;

  @Inject private GcsBlobStorageClient gcsBlobStorageClient;

  private static final String TEST_STRING = "GCS integration test with testcontainers";

  private static final String GCP_BUCKET = "gcp_bucket";

  private File file;

  @Before
  public void setUp() throws IOException {
    // Create text file to upload.
    try {
      file = folder.newFile("file.txt");
      FileWriter writer = new FileWriter(file, Charset.forName("UTF-8"));
      writer.write(TEST_STRING);
      writer.close();
    } catch (IOException e) {
      e.printStackTrace();
      throw new IOException("Could not create test file in temp dir.");
    }
  }

  @Test
  public void getBlob_consistentObjectRetrieval() throws IOException, BlobStorageClientException {
    String bucketName = GCP_BUCKET + "1";
    gcsStorage.create(BucketInfo.newBuilder(bucketName).build());
    HashFunction hashFunction = Hashing.sha512();
    HashCode hashPreUpload = hashFunction.hashString(TEST_STRING, UTF_8);
    ByteArrayOutputStream downloadedBytesStream = new ByteArrayOutputStream(64);

    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(bucketName, "keyname"));
    gcsBlobStorageClient.putBlob(location, file.toPath());
    InputStream download = gcsBlobStorageClient.getBlob(location);
    BufferedInputStream input = new BufferedInputStream(download);
    InputStream sizedInputStream = ByteStreams.limit(input, 64);
    ByteStreams.copy(sizedInputStream, downloadedBytesStream);
    downloadedBytesStream.flush();
    downloadedBytesStream.close();
    ByteBuffer downloadedBytesBuffer = ByteBuffer.wrap(downloadedBytesStream.toByteArray());
    HashCode hashPostUpload = hashFunction.hashBytes(downloadedBytesBuffer);
    assertThat(hashPreUpload).isEqualTo(hashPostUpload);
  }

  @Test
  public void getBlob_exceptionOnMissingObject() throws Exception {
    String bucketName = GCP_BUCKET + "2";
    gcsStorage.create(BucketInfo.newBuilder(bucketName).build());

    BlobStorageClientException exception = assertThrows(
        BlobStorageClientException.class,
        () ->
            gcsBlobStorageClient.getBlob(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create(bucketName, "NonExistentKey"))));
    assertThat(exception.getReason()).isEqualTo(ErrorReason.BLOB_NOT_FOUND);

  }

  /** The main guice module used for this test. */
  public static final class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      install(new LocalGcsContainerTestModule());
    }

    @Provides
    @Inject
    @Singleton
    public GcsBlobStorageClient providesGcsBlobStorageClient(Storage gcsStorage) {
      return new GcsBlobStorageClient(gcsStorage);
    }
  }
}
