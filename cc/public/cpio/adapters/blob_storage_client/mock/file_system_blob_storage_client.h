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

#pragma once

#include <algorithm>
#include <bitset>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/strip.h"
#include "cpio/client_providers/blob_storage_client_provider/src/common/error_codes.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

namespace google::scp::cpio {

class FileSystemBlobStorageClient : public BlobStorageClientInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  void GetBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::GetBlobResponse>&
          get_blob_context) noexcept override {
    auto full_path = get_blob_context.request->blob_metadata().bucket_name() +
                     std::string("/") +
                     get_blob_context.request->blob_metadata().blob_name();

    std::ifstream input_stream(full_path, std::ios::binary | std::ios::ate);

    if (!input_stream) {
      get_blob_context.result = core::FailureExecutionResult(
          core::errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB);
      get_blob_context.Finish();
      return core::SuccessExecutionResult();
    }

    auto end_offset = input_stream.tellg();
    input_stream.seekg(0, std::ios::beg);

    auto content_length = std::size_t(end_offset - input_stream.tellg());
    get_blob_context.response = std::make_shared<
        cmrt::sdk::blob_storage_service::v1::GetBlobResponse>();

    if (content_length != 0) {
      get_blob_context.response->mutable_blob()->mutable_data()->resize(
          content_length);
      if (!input_stream.read(
              reinterpret_cast<char*>(get_blob_context.response->mutable_blob()
                                          ->mutable_data()
                                          ->data()),
              content_length)) {
        get_blob_context.result = core::FailureExecutionResult(
            core::errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB);
        get_blob_context.Finish();
        return core::SuccessExecutionResult();
      }
    }

    get_blob_context.result = core::SuccessExecutionResult();
    get_blob_context.Finish();
  }

  core::ExecutionResultOr<cmrt::sdk::blob_storage_service::v1::GetBlobResponse>
  GetBlobSync(cmrt::sdk::blob_storage_service::v1::GetBlobRequest
                  request) noexcept override {
    cmrt::sdk::blob_storage_service::v1::GetBlobResponse response;
    RETURN_IF_FAILURE(
        SyncUtils::AsyncToSync2(std::bind(&FileSystemBlobStorageClient::GetBlob,
                                          this, std::placeholders::_1),
                                std::move(request), response));
    return response;
  }

  void ListBlobsMetadata(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>&
          list_blobs_context) noexcept override {
    list_blobs_context.result = core::FailureExecutionResult(SC_UNKNOWN);
    list_blobs_context.Finish();
  }

  core::ExecutionResultOr<
      cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>
  ListBlobsMetadataSync(
      cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest
          request) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  void PutBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::PutBlobResponse>&
          put_blob_context) noexcept override {
    auto full_path = put_blob_context.request->blob().metadata().bucket_name() +
                     std::string("/") +
                     put_blob_context.request->blob().metadata().blob_name();

    std::filesystem::path storage_path(full_path);
    std::filesystem::create_directories(storage_path.parent_path());

    std::ofstream output_stream(full_path, std::ofstream::trunc);
    output_stream.write(reinterpret_cast<const char*>(
                            put_blob_context.request->blob().data().data()),
                        put_blob_context.request->blob().data().size());
    output_stream.close();

    put_blob_context.result = core::SuccessExecutionResult();
    put_blob_context.response = std::make_shared<
        cmrt::sdk::blob_storage_service::v1::PutBlobResponse>();
    put_blob_context.Finish();
  }

  core::ExecutionResultOr<cmrt::sdk::blob_storage_service::v1::PutBlobResponse>
  PutBlobSync(cmrt::sdk::blob_storage_service::v1::PutBlobRequest
                  request) noexcept override {
    cmrt::sdk::blob_storage_service::v1::PutBlobResponse response;
    RETURN_IF_FAILURE(
        SyncUtils::AsyncToSync2(std::bind(&FileSystemBlobStorageClient::PutBlob,
                                          this, std::placeholders::_1),
                                std::move(request), response));
    return response;
  }

  void DeleteBlob(core::AsyncContext<
                  cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
                  cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>&
                      delete_blob_context) noexcept override {
    delete_blob_context.result = core::FailureExecutionResult(SC_UNKNOWN);
    delete_blob_context.Finish();
  }

  core::ExecutionResultOr<
      cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>
  DeleteBlobSync(cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest
                     request) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  void GetBlobStream(
      core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
          get_blob_stream_context) noexcept override {
    get_blob_stream_context.result = core::FailureExecutionResult(SC_UNKNOWN);
    get_blob_stream_context.MarkDone();
    get_blob_stream_context.Finish();
  }

  void PutBlobStream(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          pub_blob_stream_context) noexcept override {
    pub_blob_stream_context.result = core::FailureExecutionResult(SC_UNKNOWN);
    pub_blob_stream_context.MarkDone();
    pub_blob_stream_context.Finish();
  }

  core::ExecutionResultOr<std::unique_ptr<std::ostream>> PutBlobStreamSync(
      const cmrt::sdk::blob_storage_service::v1::BlobIdentity&
          blob_identity) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResultOr<std::unique_ptr<std::istream>> GetBlobStreamSync(
      const cmrt::sdk::blob_storage_service::v1::BlobIdentity&
          blob_identity) noexcept override {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }
};

}  // namespace google::scp::cpio
