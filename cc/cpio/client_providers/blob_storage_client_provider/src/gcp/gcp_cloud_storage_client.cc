/*
 * Copyright 2026 Google LLC
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

#include "cpio/client_providers/blob_storage_client_provider/src/gcp/gcp_cloud_storage_client.h"

#include <utility>

namespace google::scp::cpio::client_providers {

using google::cloud::Status;
using google::cloud::StatusOr;
using google::cloud::storage::Client;
using google::cloud::storage::DisableCrc32cChecksum;
using google::cloud::storage::DisableMD5Hash;
using google::cloud::storage::ListObjectsReader;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::ObjectReadStream;
using google::cloud::storage::ObjectWriteStream;
using google::cloud::storage::Prefix;
using google::cloud::storage::ReadRange;
using google::cloud::storage::StartOffset;
using google::cloud::storage::UseResumableUploadSession;
using std::string;

GcpCloudStorageClient::GcpCloudStorageClient(Client client)
    : client_(std::move(client)) {}

ObjectReadStream GcpCloudStorageClient::ReadObject(
    const string& bucket_name, const string& object_name,
    DisableCrc32cChecksum disable_crc, DisableMD5Hash enable_md5) {
  return client_.ReadObject(bucket_name, object_name, disable_crc, enable_md5);
}

ObjectReadStream GcpCloudStorageClient::ReadObject(
    const string& bucket_name, const string& object_name,
    DisableCrc32cChecksum disable_crc, DisableMD5Hash enable_md5,
    ReadRange read_range) {
  return client_.ReadObject(bucket_name, object_name, disable_crc, enable_md5,
                            read_range);
}

ListObjectsReader GcpCloudStorageClient::ListObjects(const string& bucket_name,
                                                     Prefix prefix,
                                                     MaxResults max_results) {
  return client_.ListObjects(bucket_name, prefix, max_results);
}

ListObjectsReader GcpCloudStorageClient::ListObjects(const string& bucket_name,
                                                     Prefix prefix,
                                                     StartOffset start_offset,
                                                     MaxResults max_results) {
  return client_.ListObjects(bucket_name, prefix, start_offset, max_results);
}

StatusOr<ObjectMetadata> GcpCloudStorageClient::InsertObject(
    const string& bucket_name, const string& object_name, string contents,
    MD5HashValue md5_hash) {
  return client_.InsertObject(bucket_name, object_name, std::move(contents),
                              md5_hash);
}

ObjectWriteStream GcpCloudStorageClient::WriteObject(
    const string& bucket_name, const string& object_name,
    UseResumableUploadSession session) {
  return client_.WriteObject(bucket_name, object_name, std::move(session));
}

ObjectWriteStream GcpCloudStorageClient::WriteObject(
    const string& bucket_name, const string& object_name) {
  return client_.WriteObject(bucket_name, object_name);
}

Status GcpCloudStorageClient::DeleteResumableUpload(const string& session_id) {
  return client_.DeleteResumableUpload(session_id);
}

Status GcpCloudStorageClient::DeleteObject(const string& bucket_name,
                                           const string& object_name) {
  return client_.DeleteObject(bucket_name, object_name);
}

}  // namespace google::scp::cpio::client_providers
