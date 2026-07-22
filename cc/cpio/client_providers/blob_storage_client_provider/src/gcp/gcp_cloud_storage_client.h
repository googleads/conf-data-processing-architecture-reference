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

#pragma once

#include <memory>
#include <string>

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/storage/client.h"

namespace google::scp::cpio::client_providers {

class GcpCloudStorageClientInterface {
 public:
  virtual ~GcpCloudStorageClientInterface() = default;

  virtual google::cloud::storage::ObjectReadStream ReadObject(
      const std::string& bucket_name, const std::string& object_name,
      google::cloud::storage::DisableCrc32cChecksum disable_crc,
      google::cloud::storage::DisableMD5Hash enable_md5) = 0;

  virtual google::cloud::storage::ObjectReadStream ReadObject(
      const std::string& bucket_name, const std::string& object_name,
      google::cloud::storage::DisableCrc32cChecksum disable_crc,
      google::cloud::storage::DisableMD5Hash enable_md5,
      google::cloud::storage::ReadRange read_range) = 0;

  virtual google::cloud::storage::ListObjectsReader ListObjects(
      const std::string& bucket_name, google::cloud::storage::Prefix prefix,
      google::cloud::storage::MaxResults max_results) = 0;

  virtual google::cloud::storage::ListObjectsReader ListObjects(
      const std::string& bucket_name, google::cloud::storage::Prefix prefix,
      google::cloud::storage::StartOffset start_offset,
      google::cloud::storage::MaxResults max_results) = 0;

  virtual google::cloud::StatusOr<google::cloud::storage::ObjectMetadata>
  InsertObject(const std::string& bucket_name, const std::string& object_name,
               std::string contents,
               google::cloud::storage::MD5HashValue md5_hash) = 0;

  virtual google::cloud::storage::ObjectWriteStream WriteObject(
      const std::string& bucket_name, const std::string& object_name,
      google::cloud::storage::UseResumableUploadSession session) = 0;

  virtual google::cloud::storage::ObjectWriteStream WriteObject(
      const std::string& bucket_name, const std::string& object_name) = 0;

  virtual google::cloud::Status DeleteResumableUpload(
      const std::string& session_id) = 0;

  virtual google::cloud::Status DeleteObject(
      const std::string& bucket_name, const std::string& object_name) = 0;
};

class GcpCloudStorageClient : public GcpCloudStorageClientInterface {
 public:
  explicit GcpCloudStorageClient(google::cloud::storage::Client client);

  google::cloud::storage::ObjectReadStream ReadObject(
      const std::string& bucket_name, const std::string& object_name,
      google::cloud::storage::DisableCrc32cChecksum disable_crc,
      google::cloud::storage::DisableMD5Hash enable_md5) override;

  google::cloud::storage::ObjectReadStream ReadObject(
      const std::string& bucket_name, const std::string& object_name,
      google::cloud::storage::DisableCrc32cChecksum disable_crc,
      google::cloud::storage::DisableMD5Hash enable_md5,
      google::cloud::storage::ReadRange read_range) override;

  google::cloud::storage::ListObjectsReader ListObjects(
      const std::string& bucket_name, google::cloud::storage::Prefix prefix,
      google::cloud::storage::MaxResults max_results) override;

  google::cloud::storage::ListObjectsReader ListObjects(
      const std::string& bucket_name, google::cloud::storage::Prefix prefix,
      google::cloud::storage::StartOffset start_offset,
      google::cloud::storage::MaxResults max_results) override;

  google::cloud::StatusOr<google::cloud::storage::ObjectMetadata> InsertObject(
      const std::string& bucket_name, const std::string& object_name,
      std::string contents,
      google::cloud::storage::MD5HashValue md5_hash) override;

  google::cloud::storage::ObjectWriteStream WriteObject(
      const std::string& bucket_name, const std::string& object_name,
      google::cloud::storage::UseResumableUploadSession session) override;

  google::cloud::storage::ObjectWriteStream WriteObject(
      const std::string& bucket_name, const std::string& object_name) override;

  google::cloud::Status DeleteResumableUpload(
      const std::string& session_id) override;

  google::cloud::Status DeleteObject(const std::string& bucket_name,
                                     const std::string& object_name) override;

 private:
  google::cloud::storage::Client client_;
};

}  // namespace google::scp::cpio::client_providers
