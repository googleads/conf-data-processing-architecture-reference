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

class InMemoryBlobStorageClient : public BlobStorageClientInterface {
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
    std::shared_lock lock(blobs_mutex_);
    auto blob_path =
        GetBlobPath(get_blob_context.request->blob_metadata().bucket_name(),
                    get_blob_context.request->blob_metadata().blob_name());

    if (!blobs_.contains(blob_path)) {
      lock.unlock();
      get_blob_context.result = core::FailureExecutionResult(
          core::errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND);
      get_blob_context.Finish();
      return;
    }

    get_blob_context.response = std::make_shared<
        cmrt::sdk::blob_storage_service::v1::GetBlobResponse>();

    get_blob_context.response->mutable_blob()->set_data(blobs_[blob_path]);
    lock.unlock();

    get_blob_context.result = core::SuccessExecutionResult();
    get_blob_context.Finish();

    return;
  }

  core::ExecutionResultOr<cmrt::sdk::blob_storage_service::v1::GetBlobResponse>
  GetBlobSync(cmrt::sdk::blob_storage_service::v1::GetBlobRequest
                  request) noexcept override {
    cmrt::sdk::blob_storage_service::v1::GetBlobResponse response;
    RETURN_IF_FAILURE(
        SyncUtils::AsyncToSync2(std::bind(&InMemoryBlobStorageClient::GetBlob,
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
    std::unique_lock lock(blobs_mutex_);
    auto blob_path =
        GetBlobPath(put_blob_context.request->blob().metadata().bucket_name(),

                    put_blob_context.request->blob().metadata().blob_name());

    blobs_[blob_path] =
        *put_blob_context.request->mutable_blob()->mutable_data();
    lock.unlock();

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
        SyncUtils::AsyncToSync2(std::bind(&InMemoryBlobStorageClient::PutBlob,
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
          put_blob_stream_context) noexcept override {
    put_blob_stream_context.result = core::FailureExecutionResult(SC_UNKNOWN);
    put_blob_stream_context.MarkDone();
    put_blob_stream_context.Finish();
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

  static std::string GetBlobPath(std::string_view bucket_name,
                                 std::string_view blob_name) noexcept {
    return absl::StrCat(bucket_name, "/", blob_name);
  }

  absl::flat_hash_map<std::string, std::string> blobs_;

 private:
  std::shared_mutex blobs_mutex_;
};

}  // namespace google::scp::cpio
