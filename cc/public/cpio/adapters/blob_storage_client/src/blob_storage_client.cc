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

#include "blob_storage_client.h"

#include <functional>
#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/type_def.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"
#include "public/cpio/utils/sync_utils/src/sync_utils.h"

using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::BlobStorageClientProviderFactory;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::placeholders::_1;

namespace {
constexpr char kBlobStorageClient[] = "BlobStorageClient";
}  // namespace

namespace google::scp::cpio {

ExecutionResult BlobStorageClient::Init() noexcept {
  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  auto execution_result =
      GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(cpu_async_executor);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to get CpuAsyncExecutor.");

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  execution_result =
      GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to get IOAsyncExecutor.");

  shared_ptr<InstanceClientProviderInterface> instance_client;
  execution_result =
      GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(instance_client);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to get InstanceClientProvider.");

  blob_storage_client_provider_ = BlobStorageClientProviderFactory::Create(
      options_, instance_client, cpu_async_executor, io_async_executor);
  execution_result = blob_storage_client_provider_->Init();
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to initialize BlobStorageClientProvider.");

  return SuccessExecutionResult();
}

ExecutionResult BlobStorageClient::Run() noexcept {
  auto execution_result = blob_storage_client_provider_->Run();
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to run BlobStorageClientProvider.");

  return SuccessExecutionResult();
}

ExecutionResult BlobStorageClient::Stop() noexcept {
  auto execution_result = blob_storage_client_provider_->Stop();
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to stop BlobStorageClientProvider.");

  return SuccessExecutionResult();
}

void BlobStorageClient::GetBlob(
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept {
  get_blob_context.setConvertToPublicError(true);

  blob_storage_client_provider_->GetBlob(get_blob_context);
}

ExecutionResultOr<GetBlobResponse> BlobStorageClient::GetBlobSync(
    GetBlobRequest request) noexcept {
  GetBlobResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<GetBlobRequest, GetBlobResponse>(
          bind(&BlobStorageClient::GetBlob, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to get blob.");
  return response;
}

void BlobStorageClient::ListBlobsMetadata(
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>&
        list_blobs_metadata_context) noexcept {
  list_blobs_metadata_context.setConvertToPublicError(true);
  blob_storage_client_provider_->ListBlobsMetadata(list_blobs_metadata_context);
}

ExecutionResultOr<ListBlobsMetadataResponse>
BlobStorageClient::ListBlobsMetadataSync(
    ListBlobsMetadataRequest request) noexcept {
  ListBlobsMetadataResponse response;
  auto execution_result = SyncUtils::AsyncToSync2<ListBlobsMetadataRequest,
                                                  ListBlobsMetadataResponse>(
      bind(&BlobStorageClient::ListBlobsMetadata, this, _1), move(request),
      response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to list blobs metadata.");
  return response;
}

void BlobStorageClient::PutBlob(
    AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) noexcept {
  put_blob_context.setConvertToPublicError(true);
  blob_storage_client_provider_->PutBlob(put_blob_context);
}

ExecutionResultOr<PutBlobResponse> BlobStorageClient::PutBlobSync(
    PutBlobRequest request) noexcept {
  PutBlobResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<PutBlobRequest, PutBlobResponse>(
          bind(&BlobStorageClient::PutBlob, this, _1), move(request), response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to put blob.");
  return response;
}

void BlobStorageClient::DeleteBlob(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
        delete_blob_context) noexcept {
  delete_blob_context.setConvertToPublicError(true);
  blob_storage_client_provider_->DeleteBlob(delete_blob_context);
}

ExecutionResultOr<DeleteBlobResponse> BlobStorageClient::DeleteBlobSync(
    DeleteBlobRequest request) noexcept {
  DeleteBlobResponse response;
  auto execution_result =
      SyncUtils::AsyncToSync2<DeleteBlobRequest, DeleteBlobResponse>(
          bind(&BlobStorageClient::DeleteBlob, this, _1), move(request),
          response);
  RETURN_AND_LOG_IF_FAILURE(ConvertToPublicExecutionResult(execution_result),
                            kBlobStorageClient, kZeroUuid,
                            "Failed to delete blob.");
  return response;
}

void BlobStorageClient::GetBlobStream(
    ConsumerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
        GetBlobStreamResponse>& get_blob_stream_context) noexcept {
  get_blob_stream_context.setConvertToPublicError(true);
  blob_storage_client_provider_->GetBlobStream(get_blob_stream_context);
}

void BlobStorageClient::PutBlobStream(
    ProducerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
        PutBlobStreamResponse>& put_blob_stream_context) noexcept {
  put_blob_stream_context.setConvertToPublicError(true);
  blob_storage_client_provider_->PutBlobStream(put_blob_stream_context);
}

ExecutionResultOr<std::unique_ptr<std::ostream>>
BlobStorageClient::PutBlobStreamSync(
    const cmrt::sdk::blob_storage_service::v1::BlobIdentity&
        blob_identity) noexcept {
  return blob_storage_client_provider_->PutBlobStreamSync(blob_identity);
};

ExecutionResultOr<std::unique_ptr<std::istream>>
BlobStorageClient::GetBlobStreamSync(
    const cmrt::sdk::blob_storage_service::v1::BlobIdentity&
        blob_identity) noexcept {
  return blob_storage_client_provider_->GetBlobStreamSync(blob_identity);
};

std::unique_ptr<BlobStorageClientInterface> BlobStorageClientFactory::Create(
    BlobStorageClientOptions options) {
  return make_unique<BlobStorageClient>(
      make_shared<BlobStorageClientOptions>(options));
}
}  // namespace google::scp::cpio
