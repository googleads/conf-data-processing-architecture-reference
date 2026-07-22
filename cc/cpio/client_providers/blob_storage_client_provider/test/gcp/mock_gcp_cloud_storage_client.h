// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <gmock/gmock.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cpio/client_providers/blob_storage_client_provider/src/gcp/gcp_blob_storage_client_provider.h"
#include "cpio/client_providers/blob_storage_client_provider/src/gcp/gcp_cloud_storage_client.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/object_write_streambuf.h"

namespace google::scp::cpio::client_providers::mock {

class MockObjectReadSource
    : public ::google::cloud::storage::internal::ObjectReadSource {
 public:
  MOCK_METHOD(bool, IsOpen, (), (const, override));
  MOCK_METHOD((::google::cloud::StatusOr<
                  ::google::cloud::storage::internal::HttpResponse>),
              Close, (), (override));
  MOCK_METHOD((::google::cloud::StatusOr<
                  ::google::cloud::storage::internal::ReadSourceResult>),
              Read, (char* buf, std::size_t n), (override));
};

class MockObjectWriteStreambuf
    : public ::google::cloud::storage::internal::ObjectWriteStreambuf {
 public:
  explicit MockObjectWriteStreambuf(std::string session_id = "test-session-id")
      : session_id_(std::move(session_id)) {}

  bool IsOpen() const override { return is_open_; }

  ::google::cloud::StatusOr<
      ::google::cloud::storage::internal::QueryResumableUploadResponse>
  Close() override {
    is_open_ = false;
    return ::google::cloud::storage::internal::QueryResumableUploadResponse{
        0, ::google::cloud::storage::ObjectMetadata()};
  }

  std::string const& resumable_session_id() const override {
    return session_id_;
  }

 protected:
  std::streamsize xsputn(char const* s, std::streamsize count) override {
    buffer_.append(s, count);
    return count;
  }

  int_type overflow(int_type ch) override {
    if (ch != EOF) buffer_.push_back(ch);
    return ch;
  }

 private:
  std::string session_id_;
  std::string buffer_;
  bool is_open_ = true;
};

class MockGcpCloudStorageClient : public GcpCloudStorageClientInterface {
 public:
  MockGcpCloudStorageClient() {
    ON_CALL(*this, ReadObject(::testing::_, ::testing::_, ::testing::_,
                              ::testing::_, ::testing::_))
        .WillByDefault([this](
                           const std::string& bucket_name,
                           const std::string& object_name,
                           ::google::cloud::storage::DisableCrc32cChecksum
                               disable_crc,
                           ::google::cloud::storage::DisableMD5Hash enable_md5,
                           ::google::cloud::storage::ReadRange read_range) {
          if (!read_range.has_value() ||
              (read_range.value().begin == 0 && read_range.value().end == 0)) {
            return ReadObject(bucket_name, object_name, disable_crc,
                              enable_md5);
          }
          return ::google::cloud::storage::ObjectReadStream(
              std::make_unique<
                  ::google::cloud::storage::internal::ObjectReadStreambuf>(
                  ::google::cloud::storage::internal::ReadObjectRangeRequest(),
                  ::google::cloud::Status(
                      ::google::cloud::StatusCode::kUnimplemented,
                      "GCP unimplemented")));
        });
  }

  MOCK_METHOD(::google::cloud::storage::ObjectReadStream, ReadObject,
              (const std::string&, const std::string&,
               ::google::cloud::storage::DisableCrc32cChecksum,
               ::google::cloud::storage::DisableMD5Hash),
              (override));
  MOCK_METHOD(::google::cloud::storage::ObjectReadStream, ReadObject,
              (const std::string&, const std::string&,
               ::google::cloud::storage::DisableCrc32cChecksum,
               ::google::cloud::storage::DisableMD5Hash,
               ::google::cloud::storage::ReadRange),
              (override));
  MOCK_METHOD(::google::cloud::storage::ListObjectsReader, ListObjects,
              (const std::string&, ::google::cloud::storage::Prefix,
               ::google::cloud::storage::MaxResults),
              (override));
  MOCK_METHOD(::google::cloud::storage::ListObjectsReader, ListObjects,
              (const std::string&, ::google::cloud::storage::Prefix,
               ::google::cloud::storage::StartOffset,
               ::google::cloud::storage::MaxResults),
              (override));
  MOCK_METHOD(
      (::google::cloud::StatusOr<::google::cloud::storage::ObjectMetadata>),
      InsertObject,
      (const std::string&, const std::string&, std::string,
       ::google::cloud::storage::MD5HashValue),
      (override));
  MOCK_METHOD(::google::cloud::storage::ObjectWriteStream, WriteObject,
              (const std::string&, const std::string&,
               ::google::cloud::storage::UseResumableUploadSession),
              (override));
  MOCK_METHOD(::google::cloud::storage::ObjectWriteStream, WriteObject,
              (const std::string&, const std::string&), (override));
  MOCK_METHOD(::google::cloud::Status, DeleteResumableUpload,
              (const std::string&), (override));
  MOCK_METHOD(::google::cloud::Status, DeleteObject,
              (const std::string&, const std::string&), (override));
};

class MockGcpCloudStorageFactory : public GcpCloudStorageFactory {
 public:
  MOCK_METHOD((core::ExecutionResultOr<
                  std::shared_ptr<GcpCloudStorageClientInterface>>),
              CreateClient,
              (std::shared_ptr<BlobStorageClientOptions>, const std::string&,
               const std::string&),
              (noexcept, override));
};

inline ::google::cloud::StatusOr<
    std::unique_ptr<::google::cloud::storage::internal::ObjectReadSource>>
BuildReadResponseFromString(const std::string& bytes_str) {
  auto mock_source = std::make_unique<MockObjectReadSource>();
  auto offset = std::make_shared<std::size_t>(0);
  EXPECT_CALL(*mock_source, IsOpen)
      .WillRepeatedly(
          [offset, length = bytes_str.length()]() { return *offset < length; });
  EXPECT_CALL(*mock_source, Close)
      .WillRepeatedly(::testing::Return(
          ::google::cloud::storage::internal::HttpResponse{200, {}, {}}));
  EXPECT_CALL(*mock_source, Read)
      .WillRepeatedly([bytes_str = bytes_str, offset](void* buf,
                                                      std::size_t n) {
        std::multimap<std::string, std::string> headers = {
            {"x-goog-stored-content-length",
             std::to_string(bytes_str.length())},
            {"content-length", std::to_string(bytes_str.length())}};
        if (*offset >= bytes_str.length()) {
          core::BytesBuffer buffer(bytes_str.length());
          buffer.bytes->assign(bytes_str.begin(), bytes_str.end());
          buffer.length = bytes_str.length();
          ::google::cloud::storage::internal::ReadSourceResult result{
              0, ::google::cloud::storage::internal::HttpResponse{
                     200, {}, headers}};
          result.hashes.md5 = *core::utils::CalculateMd5Hash(buffer);
          result.hashes.md5 = *core::utils::Base64Encode(result.hashes.md5);
          result.size = bytes_str.length();
          return result;
        }
        core::BytesBuffer buffer(bytes_str.length());
        buffer.bytes->assign(bytes_str.begin(), bytes_str.end());
        buffer.length = bytes_str.length();
        auto length = std::min(bytes_str.length() - *offset, n);
        std::memcpy(buf, buffer.bytes->data() + *offset, length);
        *offset += length;
        ::google::cloud::storage::internal::ReadSourceResult result{
            length,
            ::google::cloud::storage::internal::HttpResponse{200, {}, headers}};
        result.hashes.md5 = *core::utils::CalculateMd5Hash(buffer);
        result.hashes.md5 = *core::utils::Base64Encode(result.hashes.md5);
        result.size = bytes_str.length();
        return result;
      });
  return std::unique_ptr<::google::cloud::storage::internal::ObjectReadSource>(
      std::move(mock_source));
}

inline ::google::cloud::storage::ObjectReadStream BuildReadStreamFromString(
    const std::string& bytes_str) {
  auto source = BuildReadResponseFromString(bytes_str);
  ::google::cloud::storage::ObjectReadStream stream(
      std::make_unique<::google::cloud::storage::internal::ObjectReadStreambuf>(
          ::google::cloud::storage::internal::ReadObjectRangeRequest(),
          std::move(*source)));
  stream.peek();
  return stream;
}

inline ::google::cloud::storage::ObjectReadStream BuildReadStreamFromStatus(
    const ::google::cloud::Status& status) {
  return ::google::cloud::storage::ObjectReadStream(
      std::make_unique<::google::cloud::storage::internal::ObjectReadStreambuf>(
          ::google::cloud::storage::internal::ReadObjectRangeRequest(),
          status));
}

inline ::google::cloud::storage::ObjectWriteStream BuildObjectWriteStream(
    std::string session_id = "test-session-id") {
  return ::google::cloud::storage::ObjectWriteStream(
      std::make_unique<MockObjectWriteStreambuf>(std::move(session_id)));
}

inline ::google::cloud::storage::ObjectWriteStream
BuildObjectWriteStreamFromStatus(const ::google::cloud::Status& status) {
  return ::google::cloud::storage::ObjectWriteStream(
      std::make_unique<
          ::google::cloud::storage::internal::ObjectWriteStreambuf>(status));
}

inline ::google::cloud::storage::ListObjectsReader BuildListObjectsReader(
    ::google::cloud::StatusOr<
        ::google::cloud::storage::internal::ListObjectsResponse>
        response_or) {
  if (!response_or.ok()) {
    return ::google::cloud::internal::MakeErrorPaginationRange<
        ::google::cloud::storage::ListObjectsReader>(response_or.status());
  }
  auto response = *response_or;
  auto loader =
      [response](::google::cloud::storage::internal::ListObjectsRequest const&)
      -> ::google::cloud::StatusOr<
          ::google::cloud::storage::internal::ListObjectsResponse> {
    return response;
  };
  auto extractor = [](::google::cloud::storage::internal::ListObjectsResponse r)
      -> std::vector<::google::cloud::storage::ObjectMetadata> {
    return r.items;
  };
  return ::google::cloud::internal::MakePaginationRange<
      ::google::cloud::storage::ListObjectsReader>(
      ::google::cloud::storage::internal::ListObjectsRequest{}, loader,
      extractor);
}

inline ::google::cloud::storage::ListObjectsReader
BuildListObjectsReaderFromStatus(const ::google::cloud::Status& status) {
  return ::google::cloud::internal::MakeErrorPaginationRange<
      ::google::cloud::storage::ListObjectsReader>(status);
}

}  // namespace google::scp::cpio::client_providers::mock
