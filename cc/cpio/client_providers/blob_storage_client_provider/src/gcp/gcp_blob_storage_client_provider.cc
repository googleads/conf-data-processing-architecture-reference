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

#include "gcp_blob_storage_client_provider.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/time_util.h>

#include "absl/strings/str_format.h"
#include "cc/core/interface/configuration_keys.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/type_def.h"
#include "core/utils/src/hashing.h"
#include "cpio/client_providers/blob_storage_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/object_read_stream.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/type_def.h"

#include "gcp_blob_storage_client_utils.h"

using google::cloud::MakeExternalAccountCredentials;
using google::cloud::Options;
using google::cloud::StatusCode;
using google::cloud::StatusOr;
using google::cloud::UnifiedCredentialsOption;
using google::cloud::storage::Client;
using google::cloud::storage::ComputeMD5Hash;
using google::cloud::storage::ConnectionPoolSizeOption;
using google::cloud::storage::DisableCrc32cChecksum;
using google::cloud::storage::EnableMD5Hash;
using google::cloud::storage::IdempotencyPolicyOption;
using google::cloud::storage::LimitedErrorCountRetryPolicy;
using google::cloud::storage::ListObjectsReader;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::NewResumableUploadSession;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::ObjectReadStream;
using google::cloud::storage::ObjectWriteStream;
using google::cloud::storage::Prefix;
using google::cloud::storage::ProjectIdOption;
using google::cloud::storage::ReadRange;
using google::cloud::storage::RestoreResumableUploadSession;
using google::cloud::storage::RetryPolicyOption;
using google::cloud::storage::StartOffset;
using google::cloud::storage::StrictIdempotencyPolicy;
using google::cloud::storage::TransferStallTimeoutOption;
using google::cmrt::sdk::blob_storage_service::v1::Blob;
using google::cmrt::sdk::blob_storage_service::v1::BlobMetadata;
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
using google::cmrt::sdk::common::v1::CloudIdentityInfo;
using google::protobuf::MessageLite;
using google::protobuf::io::CodedOutputStream;
using google::protobuf::io::StringOutputStream;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FinishContext;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_ERROR_INVALID_GET_BLOB_STREAM;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_ERROR_INVALID_PUT_BLOB_STREAM;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_INVALID_CACHED_CLIENT_LIFETIME;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED;

using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::common::GcpUtils;

using std::bind;
using std::function;
using std::ios_base;
using std::istream;
using std::make_shared;
using std::make_unique;
using std::min;
using std::move;
using std::ostream;
using std::ref;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::minutes;
using std::chrono::nanoseconds;
using std::chrono::seconds;

namespace {

constexpr size_t kMaxConcurrentConnections = 1000;
constexpr size_t kListBlobsMetadataMaxResults = 1000;
constexpr size_t k64KbCount = 64 << 10;
constexpr size_t kMaxSizeBytesToRead = std::numeric_limits<size_t>::max();
constexpr nanoseconds kDefaultStreamKeepaliveNanos =
    duration_cast<nanoseconds>(minutes(5));
constexpr nanoseconds kMaximumStreamKeepaliveNanos =
    duration_cast<nanoseconds>(minutes(10));
constexpr seconds kPutBlobRescanTime = seconds(5);

bool IsPageTokenObject(const ListBlobsMetadataRequest& list_blobs_request,
                       const ObjectMetadata& obj_metadata) {
  return list_blobs_request.has_page_token() &&
         list_blobs_request.page_token() == obj_metadata.name();
}

uint64_t GetMaxPageSize(const ListBlobsMetadataRequest& list_blobs_request) {
  return list_blobs_request.has_max_page_size()
             ? list_blobs_request.max_page_size()
             : kListBlobsMetadataMaxResults;
}

// Given a single binary build, this function ensures two protos with identical
// content will serialize to same bytes.
string SerializeDeterministically(const MessageLite& message) {
  string serializedString;
  {
    StringOutputStream output_stream(&serializedString);
    CodedOutputStream coded_stream(&output_stream);
    coded_stream.SetSerializationDeterministic(true);
    message.SerializePartialToCodedStream(&coded_stream);
  }
  return serializedString;
}

}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult GcpBlobStorageClientProvider::Init() noexcept {
  if (options_->cached_client_lifetime.count() == 0) {
    auto execution_result = FailureExecutionResult(
        SC_BLOB_STORAGE_PROVIDER_INVALID_CACHED_CLIENT_LIFETIME);
    SCP_ERROR(
        kGcpBlobStorageClientProvider, kZeroUuid, execution_result,
        "Invalid cached client lifetime %s in blob storage client options.",
        options_->cached_client_lifetime.count());
    return execution_result;
  }

  auto project_id_or =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client_);
  if (!project_id_or.Successful()) {
    SCP_ERROR(kGcpBlobStorageClientProvider, kZeroUuid, project_id_or.result(),
              "Failed to get project ID for current instance");
    return project_id_or.result();
  }
  current_project_id_ = move(*project_id_or);

  CloudIdentityInfo cloud_identity_info;
  cloud_identity_info.set_owner_id(current_project_id_);
  RETURN_IF_FAILURE(
      GetOrCreateCloudStroageClient(cloud_identity_info).result());
  return cloud_storage_client_pool_->Init();
}

ExecutionResult GcpBlobStorageClientProvider::Run() noexcept {
  return cloud_storage_client_pool_->Run();
}

ExecutionResult GcpBlobStorageClientProvider::Stop() noexcept {
  return cloud_storage_client_pool_->Stop();
}

void GcpBlobStorageClientProvider::GetBlob(
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept {
  const auto& request = *get_blob_context.request;
  if (request.blob_metadata().bucket_name().empty() ||
      request.blob_metadata().blob_name().empty()) {
    get_blob_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_context,
                      get_blob_context.result,
                      "Get blob request is missing bucket or blob name");
    get_blob_context.Finish();
    return;
  }
  if (request.has_byte_range() && request.byte_range().begin_byte_index() >
                                      request.byte_range().end_byte_index()) {
    get_blob_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, get_blob_context,
        get_blob_context.result,
        "Get blob request provides begin_byte_index that is larger "
        "than end_byte_index");
    get_blob_context.Finish();
    return;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpBlobStorageClientProvider::GetBlobInternal, this,
               get_blob_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    get_blob_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_context,
                      get_blob_context.result,
                      "Get blob request failed to be scheduled");
    get_blob_context.Finish();
  }
}

void GcpBlobStorageClientProvider::GetBlobInternal(
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context) noexcept {
  auto client_or = GetOrCreateCloudStroageClient(
      get_blob_context.request->cloud_identity_info());
  if (!client_or.Successful()) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, get_blob_context, result,
        "Create google cloud storage client failed for get blob request.");
    FinishContext(result, get_blob_context, cpu_async_executor_);
    return;
  }
  Client cloud_storage_client(*(client_or.value()));

  ReadRange read_range;
  if (get_blob_context.request->has_byte_range()) {
    // ReadRange is right-open and ByteRange::end_byte_index is said to be
    // inclusive, add one.
    read_range =
        ReadRange(get_blob_context.request->byte_range().begin_byte_index(),
                  get_blob_context.request->byte_range().end_byte_index() + 1);
  }
  ObjectReadStream blob_stream = cloud_storage_client.ReadObject(
      get_blob_context.request->blob_metadata().bucket_name(),
      get_blob_context.request->blob_metadata().blob_name(),
      DisableCrc32cChecksum(true), EnableMD5Hash(), read_range);
  if (!ValidateStream(get_blob_context, blob_stream).Successful()) {
    return;
  }

  // blob_stream.size() always has the full size of the object, not just the
  // read range.
  size_t content_length = *blob_stream.size();
  if (get_blob_context.request->has_byte_range()) {
    const size_t max_end_index = content_length - 1;
    // If the end byte is beyond the size of the object, truncate to the end of
    // the object.
    size_t end_index =
        get_blob_context.request->byte_range().end_byte_index() > max_end_index
            ? max_end_index
            : get_blob_context.request->byte_range().end_byte_index();
    content_length = 1 + end_index -
                     get_blob_context.request->byte_range().begin_byte_index();
  }

  get_blob_context.response = make_shared<GetBlobResponse>();
  get_blob_context.response->mutable_blob()->mutable_metadata()->CopyFrom(
      get_blob_context.request->blob_metadata());

  auto& blob_bytes = *get_blob_context.response->mutable_blob()->mutable_data();
  blob_bytes.resize(content_length);

  blob_stream.read(blob_bytes.data(), content_length);
  if (!ValidateStream(get_blob_context, blob_stream).Successful()) {
    return;
  }

  FinishContext(SuccessExecutionResult(), get_blob_context,
                cpu_async_executor_);
}

void GcpBlobStorageClientProvider::GetBlobStream(
    ConsumerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
        GetBlobStreamResponse>& get_blob_stream_context) noexcept {
  const auto& request = *get_blob_stream_context.request;
  if (request.blob_metadata().bucket_name().empty() ||
      request.blob_metadata().blob_name().empty()) {
    get_blob_stream_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      get_blob_stream_context.result,
                      "Get blob stream request is missing bucket or blob name");
    get_blob_stream_context.MarkDone();
    get_blob_stream_context.Finish();
    return;
  }
  if (request.has_byte_range() && request.byte_range().begin_byte_index() >
                                      request.byte_range().end_byte_index()) {
    get_blob_stream_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, get_blob_stream_context,
        get_blob_stream_context.result,
        "Get blob stream request provides begin_byte_index that is larger "
        "than end_byte_index");
    get_blob_stream_context.MarkDone();
    get_blob_stream_context.Finish();
    return;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpBlobStorageClientProvider::GetBlobStreamInternal, this,
               get_blob_stream_context, nullptr /*tracker*/),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    get_blob_stream_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      get_blob_stream_context.result,
                      "Get blob stream request failed to be scheduled");
    get_blob_stream_context.MarkDone();
    get_blob_stream_context.Finish();
  }
}

void GcpBlobStorageClientProvider::GetBlobStreamInternal(
    ConsumerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
        GetBlobStreamResponse>
        get_blob_stream_context,
    shared_ptr<GetBlobStreamTracker> tracker) noexcept {
  if (!tracker) {
    auto tracker_or = InitGetBlobStreamTracker(get_blob_stream_context);
    if (!tracker_or.Successful()) return;
    tracker = move(*tracker_or);
  }
  if (get_blob_stream_context.IsCancelled()) {
    auto result = FailureExecutionResult(
        SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      result, "Get blob stream request was cancelled.");
    FinishStreamingContext(result, get_blob_stream_context,
                           cpu_async_executor_);
    return;
  }
  auto response = ReadNextPortion(*get_blob_stream_context.request, *tracker);

  if (!ValidateStream(get_blob_stream_context, tracker->stream).Successful()) {
    return;
  }

  auto push_result = get_blob_stream_context.TryPushResponse(move(response));
  if (!push_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      push_result, "Failed to push new message.");
    FinishStreamingContext(push_result, get_blob_stream_context,
                           cpu_async_executor_);
    return;
  }

  // Schedule processing the next message.
  auto schedule_result = cpu_async_executor_->Schedule(
      [get_blob_stream_context]() mutable {
        get_blob_stream_context.ProcessNextMessage();
      },
      AsyncPriority::Normal);
  if (!schedule_result.Successful()) {
    get_blob_stream_context.result = schedule_result;
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, get_blob_stream_context,
        get_blob_stream_context.result,
        "Get blob stream process next message failed to be scheduled");
    FinishStreamingContext(schedule_result, get_blob_stream_context,
                           cpu_async_executor_);
  }

  if (tracker->remaining_bytes_count == 0) {
    FinishStreamingContext(SuccessExecutionResult(), get_blob_stream_context,
                           cpu_async_executor_);
    return;
  }

  // Schedule reading the next section.
  schedule_result = io_async_executor_->Schedule(
      bind(&GcpBlobStorageClientProvider::GetBlobStreamInternal, this,
           get_blob_stream_context, move(tracker)),
      AsyncPriority::Normal);
  if (!schedule_result.Successful()) {
    get_blob_stream_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      get_blob_stream_context.result,
                      "Get blob stream follow up read failed to be scheduled");
    FinishStreamingContext(schedule_result, get_blob_stream_context,
                           cpu_async_executor_);
  }
}

ExecutionResultOr<
    shared_ptr<GcpBlobStorageClientProvider::GetBlobStreamTracker>>
GcpBlobStorageClientProvider::InitGetBlobStreamTracker(
    core::ConsumerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
        cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
        context) noexcept {
  auto client_or =
      GetOrCreateCloudStroageClient(context.request->cloud_identity_info());
  if (!client_or.Successful()) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, context, result,
                      "Create google cloud storage client failed for get blob "
                      "stream request.");
    FinishStreamingContext(result, context, cpu_async_executor_);
    return result;
  }
  Client cloud_storage_client(*(client_or.value()));

  // Set up the tracker to the beginning.
  auto tracker = make_shared<GetBlobStreamTracker>();
  ReadRange read_range;
  if (context.request->has_byte_range()) {
    // ReadRange is right-open and ByteRange::end_byte_index is said to be
    // inclusive, add one.
    read_range = ReadRange(context.request->byte_range().begin_byte_index(),
                           context.request->byte_range().end_byte_index() + 1);
  }
  tracker->stream = cloud_storage_client.ReadObject(
      context.request->blob_metadata().bucket_name(),
      context.request->blob_metadata().blob_name(), DisableCrc32cChecksum(true),
      EnableMD5Hash(), read_range);
  auto& blob_stream = tracker->stream;
  auto validate_result = ValidateStream(context, blob_stream);
  if (!validate_result.Successful()) {
    return validate_result;
  }

  if (!blob_stream.size()) {
    SCP_DEBUG_CONTEXT(kGcpBlobStorageClientProvider, context,
                      "Get blob stream request failed. Message: size "
                      "missing. Setting size to maximum and proceeding.");
  }

  // blob_stream.size() always has the full size of the object, not just the
  // read range.
  const auto& blob_size = blob_stream.size();
  if (context.request->has_byte_range()) {
    const size_t max_end_index = blob_size.value_or(kMaxSizeBytesToRead) - 1;
    // If the end byte is beyond the size of the object, truncate to the end
    // of the object.
    size_t end_index =
        context.request->byte_range().end_byte_index() > max_end_index
            ? max_end_index
            : context.request->byte_range().end_byte_index();
    tracker->remaining_bytes_count =
        1 + end_index - context.request->byte_range().begin_byte_index();
  } else {
    tracker->remaining_bytes_count = blob_size.value_or(kMaxSizeBytesToRead);
  }
  // The first portion will start at begin_byte_index.
  tracker->last_end_byte_index =
      context.request->byte_range().begin_byte_index() - 1;
  return tracker;
}

GetBlobStreamResponse GcpBlobStorageClientProvider::ReadNextPortion(
    const cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest& request,
    GetBlobStreamTracker& tracker) noexcept {
  auto& blob_stream = tracker.stream;
  // If max_bytes_per_response is provided, use it. Otherwise use 64KB.
  size_t next_read_size = request.max_bytes_per_response() == 0
                              ? k64KbCount
                              : request.max_bytes_per_response();
  // Read up to next_read_size or bytes_remaining. If we don't know how many
  // bytes are remaining, naively read next_read_size; we will check if all
  // bytes were read via eof().
  next_read_size = min(next_read_size, tracker.remaining_bytes_count);

  GetBlobStreamResponse response;
  response.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      request.blob_metadata());
  // We begin one past where we ended last.
  response.mutable_byte_range()->set_begin_byte_index(
      tracker.last_end_byte_index + 1);
  // We end one space before the read size.
  response.mutable_byte_range()->set_end_byte_index(
      response.byte_range().begin_byte_index() + next_read_size - 1);

  auto& blob_bytes = *response.mutable_blob_portion()->mutable_data();
  blob_bytes.resize(next_read_size);

  blob_stream.read(blob_bytes.data(), next_read_size);
  if (blob_stream.eof()) {
    // We oversized blob_bytes.
    blob_bytes.resize(blob_stream.gcount());
    // This specific logic is in place in case the initial size was unable to be
    // acquired. We manually check eofbit to avoid errors. Set
    // remaining_bytes_count to 0 to indicate that we are done reading.
    tracker.remaining_bytes_count = 0;
    // Set the end_byte_index to the actual number of bytes read.
    response.mutable_byte_range()->set_end_byte_index(
        response.byte_range().begin_byte_index() + blob_stream.gcount() - 1);
  } else {
    tracker.remaining_bytes_count -= next_read_size;
    tracker.last_end_byte_index = response.byte_range().end_byte_index();
  }
  return response;
}

ExecutionResultOr<unique_ptr<istream>>
GcpBlobStorageClientProvider::GetBlobStreamSync(
    const cmrt::sdk::blob_storage_service::v1::BlobIdentity&
        blob_identity) noexcept {
  if (blob_identity.blob_metadata().bucket_name().empty() ||
      blob_identity.blob_metadata().blob_name().empty()) {
    return FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
  }

  auto client_or =
      GetOrCreateCloudStroageClient(blob_identity.cloud_identity_info());
  if (!client_or.Successful()) {
    SCP_ERROR(kGcpBlobStorageClientProvider, kZeroUuid, client_or.result(),
              "Failed creating Google Cloud Storage client.");
    return client_or.result();
  }
  Client cloud_storage_client(*(client_or.value()));

  auto blob_stream =
      make_unique<ObjectReadStream>(cloud_storage_client.ReadObject(
          blob_identity.blob_metadata().bucket_name(),
          blob_identity.blob_metadata().blob_name(),
          DisableCrc32cChecksum(true), EnableMD5Hash()));

  if (blob_stream->bad()) {
    return FailureExecutionResult(
        SC_BLOB_STORAGE_PROVIDER_ERROR_INVALID_GET_BLOB_STREAM);
  }
  return blob_stream;
}

void GcpBlobStorageClientProvider::ListBlobsMetadata(
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>&
        list_blobs_context) noexcept {
  const auto& request = *list_blobs_context.request;
  if (request.blob_metadata().bucket_name().empty()) {
    list_blobs_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, list_blobs_context,
                      list_blobs_context.result,
                      "List blobs metadata request failed. Bucket name empty.");
    list_blobs_context.Finish();
    return;
  }
  if (request.has_max_page_size() && request.max_page_size() > 1000) {
    list_blobs_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, list_blobs_context,
        list_blobs_context.result,
        "List blobs metadata request failed. Max page size cannot be "
        "greater than 1000.");
    return;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpBlobStorageClientProvider::ListBlobsMetadataInternal, this,
               list_blobs_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    list_blobs_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, list_blobs_context,
                      list_blobs_context.result,
                      "List blobs metadata request failed to be scheduled");
    list_blobs_context.Finish();
  }
}

void GcpBlobStorageClientProvider::ListBlobsMetadataInternal(
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
        list_blobs_context) noexcept {
  const auto& request = *list_blobs_context.request;
  auto client_or = GetOrCreateCloudStroageClient(request.cloud_identity_info());
  if (!client_or.Successful()) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, list_blobs_context, result,
        "Create google cloud storage client failed for list blobs metadata "
        "request.");
    FinishContext(result, list_blobs_context, cpu_async_executor_);
    return;
  }
  Client cloud_storage_client(*(client_or.value()));

  auto objects_reader = [&request, &cloud_storage_client]() {
    auto prefix = request.blob_metadata().blob_name().empty()
                      ? Prefix()
                      : Prefix(request.blob_metadata().blob_name());
    auto max_page_size = request.has_max_page_size()
                             ? request.max_page_size()
                             : kListBlobsMetadataMaxResults;
    auto max_results = MaxResults(max_page_size);
    if (!request.has_page_token() || request.page_token().empty()) {
      return cloud_storage_client.ListObjects(
          request.blob_metadata().bucket_name(), prefix, max_results);
    } else {
      return cloud_storage_client.ListObjects(
          request.blob_metadata().bucket_name(), prefix,
          StartOffset(request.page_token()), max_results);
    }
  }();
  list_blobs_context.response = make_shared<ListBlobsMetadataResponse>();

  // GCP pagination happens through the iterator. All results are returned.
  for (auto&& object_metadata : objects_reader) {
    if (!object_metadata) {
      auto execution_result =
          GcpBlobStorageClientUtils::ConvertCloudStorageErrorToExecutionResult(
              object_metadata.status().code());
      SCP_ERROR_CONTEXT(
          kGcpBlobStorageClientProvider, list_blobs_context, execution_result,
          "List blobs request failed. Error code: %d, message: %s",
          object_metadata.status().code(),
          object_metadata.status().message().c_str());
      FinishContext(execution_result, list_blobs_context, cpu_async_executor_);
      return;
    }
    // If the first item returned is the same as the marker provided to this
    // call, then skip this object. This is because it was already included in
    // a previous call.
    if (list_blobs_context.response->blob_metadatas().empty() &&
        IsPageTokenObject(request, *object_metadata)) {
      continue;
    }
    BlobMetadata blob_metadata;
    blob_metadata.set_blob_name(object_metadata->name());
    blob_metadata.set_bucket_name(request.blob_metadata().bucket_name());
    *list_blobs_context.response->add_blob_metadatas() = move(blob_metadata);
    if (list_blobs_context.response->blob_metadatas().size() ==
        GetMaxPageSize(request)) {
      // Force the page to end here, mark the final result in this page as
      // the "next" one to start at. NOTE: There is an edge case where this
      // query returns exactly GetMaxPageSize in which case a next_marker is
      // returned, but calling ListBlobs again with this next_marker will
      // actually return 0 results but the caller issued 2 RPCs. As this is
      // an unlikely edge case, we implement the
      // https://en.wikipedia.org/wiki/Ostrich_algorithm
      list_blobs_context.response->set_next_page_token(object_metadata->name());
      break;
    }
  }
  FinishContext(SuccessExecutionResult(), list_blobs_context,
                cpu_async_executor_);
}

void GcpBlobStorageClientProvider::PutBlob(
    AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) noexcept {
  const auto& request = *put_blob_context.request;
  if (request.blob().metadata().bucket_name().empty() ||
      request.blob().metadata().blob_name().empty() ||
      request.blob().data().empty()) {
    put_blob_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_context,
                      put_blob_context.result,
                      "Put blob request failed. Ensure that bucket name, blob "
                      "name, and data are present.");
    return;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpBlobStorageClientProvider::PutBlobInternal, this,
               put_blob_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    put_blob_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_context,
                      put_blob_context.result,
                      "Put blob request failed to be scheduled");
    put_blob_context.Finish();
  }
}

void GcpBlobStorageClientProvider::PutBlobInternal(
    AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context) noexcept {
  const auto& request = *put_blob_context.request;
  auto client_or = GetOrCreateCloudStroageClient(request.cloud_identity_info());
  if (!client_or.Successful()) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, put_blob_context, result,
        "Create google cloud storage client failed for put blob request.");
    FinishContext(result, put_blob_context, cpu_async_executor_);
    return;
  }
  Client cloud_storage_client(*(client_or.value()));

  string md5_hash = ComputeMD5Hash(request.blob().data());
  auto object_metadata = cloud_storage_client.InsertObject(
      request.blob().metadata().bucket_name(),
      request.blob().metadata().blob_name(), request.blob().data(),
      MD5HashValue(md5_hash));
  if (!object_metadata) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_context,
                      put_blob_context.result,
                      "Put blob request failed. Error code: %d, message: %s",
                      object_metadata.status().code(),
                      object_metadata.status().message().c_str());
    auto execution_result =
        GcpBlobStorageClientUtils::ConvertCloudStorageErrorToExecutionResult(
            object_metadata.status().code());
    FinishContext(execution_result, put_blob_context, cpu_async_executor_);
    return;
  }
  put_blob_context.response = make_shared<PutBlobResponse>();
  FinishContext(SuccessExecutionResult(), put_blob_context,
                cpu_async_executor_);
}

void GcpBlobStorageClientProvider::PutBlobStream(
    ProducerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
        PutBlobStreamResponse>& put_blob_stream_context) noexcept {
  const auto& request = *put_blob_stream_context.request;
  if (request.blob_portion().metadata().bucket_name().empty() ||
      request.blob_portion().metadata().blob_name().empty() ||
      request.blob_portion().data().empty()) {
    put_blob_stream_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, put_blob_stream_context,
        put_blob_stream_context.result,
        "Put blob stream request failed. Ensure that bucket name, blob "
        "name, and data are present.");
    put_blob_stream_context.MarkDone();
    put_blob_stream_context.Finish();
    return;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpBlobStorageClientProvider::InitPutBlobStream, this,
               put_blob_stream_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    put_blob_stream_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      put_blob_stream_context.result,
                      "Put blob stream request failed to be scheduled");
    put_blob_stream_context.MarkDone();
    put_blob_stream_context.Finish();
  }
}

void GcpBlobStorageClientProvider::InitPutBlobStream(
    ProducerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
        PutBlobStreamResponse>
        put_blob_stream_context) noexcept {
  auto client_or = GetOrCreateCloudStroageClient(
      put_blob_stream_context.request->cloud_identity_info());
  if (!client_or.Successful()) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      result,
                      "Create google cloud storage client failed for put blob "
                      "stream request.");
    FinishStreamingContext(result, put_blob_stream_context,
                           cpu_async_executor_);
    return;
  }
  Client cloud_storage_client(*(client_or.value()));
  const auto& request = *put_blob_stream_context.request;
  auto tracker = make_shared<PutBlobStreamTracker>();
  auto duration = request.has_stream_keepalive_duration()
                      ? nanoseconds(TimeUtil::DurationToNanoseconds(
                            request.stream_keepalive_duration()))
                      : kDefaultStreamKeepaliveNanos;
  if (duration > kMaximumStreamKeepaliveNanos) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, put_blob_stream_context, result,
        "Supplied keepalive duration is greater than the maximum of "
        "10 minutes.");
    FinishStreamingContext(result, put_blob_stream_context,
                           cpu_async_executor_);
    return;
  }
  tracker->expiry_time_ns =
      TimeProvider::GetWallTimestampInNanoseconds() + duration;

  tracker->bucket_name = request.blob_portion().metadata().bucket_name();
  tracker->blob_name = request.blob_portion().metadata().blob_name();
  tracker->stream = cloud_storage_client.WriteObject(
      tracker->bucket_name, tracker->blob_name, NewResumableUploadSession());
  // Write the initial data from the first request.
  tracker->stream.write(request.blob_portion().data().c_str(),
                        request.blob_portion().data().length());
  if (!ValidateStream(put_blob_stream_context, tracker->stream).Successful()) {
    return;
  }
  PutBlobStreamInternal(put_blob_stream_context, tracker);
}

void GcpBlobStorageClientProvider::RestoreUploadIfSuspended(
    PutBlobStreamTracker& tracker, Client& cloud_storage_client) noexcept {
  if (tracker.session_id.has_value()) {
    // We suspended the upload previously, pick it up here.
    tracker.stream = cloud_storage_client.WriteObject(
        tracker.bucket_name, tracker.blob_name,
        RestoreResumableUploadSession(*tracker.session_id));
    tracker.session_id.reset();
  }
}

void GcpBlobStorageClientProvider::PutBlobStreamInternal(
    ProducerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
        PutBlobStreamResponse>
        put_blob_stream_context,
    shared_ptr<PutBlobStreamTracker> tracker) noexcept {
  auto client_or = GetOrCreateCloudStroageClient(
      put_blob_stream_context.request->cloud_identity_info());
  if (!client_or.Successful()) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      result,
                      "Create google cloud storage client failed for put blob "
                      "stream request.");
    FinishStreamingContext(result, put_blob_stream_context,
                           cpu_async_executor_);
    return;
  }
  Client cloud_storage_client(*(client_or.value()));

  if (put_blob_stream_context.IsCancelled()) {
    RestoreUploadIfSuspended(*tracker, cloud_storage_client);
    auto session_id = tracker->stream.resumable_session_id();
    // Cancel any outstanding uploads.
    move(tracker->stream).Suspend();
    cloud_storage_client.DeleteResumableUpload(session_id);
    auto result = FailureExecutionResult(
        SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      result, "Put blob stream request was cancelled");
    FinishStreamingContext(result, put_blob_stream_context,
                           cpu_async_executor_);
    return;
  }

  // If there's no message, schedule again. If there's a message - write it.
  auto request = put_blob_stream_context.TryGetNextRequest();
  if (request == nullptr) {
    if (put_blob_stream_context.IsMarkedDone()) {
      RestoreUploadIfSuspended(*tracker, cloud_storage_client);
      // We've processed all messages and there won't be any more.
      tracker->stream.Close();
      auto object_metadata = tracker->stream.metadata();
      auto result = SuccessExecutionResult();
      if (!object_metadata) {
        result = GcpBlobStorageClientUtils::
            ConvertCloudStorageErrorToExecutionResult(
                object_metadata.status().code());
        SCP_ERROR_CONTEXT(
            kGcpBlobStorageClientProvider, put_blob_stream_context, result,
            "Put blob stream request failed. Error code: %d, message: %s",
            object_metadata.status().code(),
            object_metadata.status().message().c_str());
      }
      put_blob_stream_context.response = make_shared<PutBlobStreamResponse>();
      FinishStreamingContext(result, put_blob_stream_context,
                             cpu_async_executor_);
      return;
    }
    // If this session expired, cancel the upload and finish.
    if (TimeProvider::GetWallTimestampInNanoseconds() >=
        tracker->expiry_time_ns) {
      auto result = FailureExecutionResult(
          SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED);
      SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                        result, "Put blob stream session expired.");
      auto session_id =
          tracker->session_id.value_or(tracker->stream.resumable_session_id());
      // Cancel any outstanding uploads.
      move(tracker->stream).Suspend();
      cloud_storage_client.DeleteResumableUpload(session_id);
      FinishStreamingContext(result, put_blob_stream_context,
                             cpu_async_executor_);
      return;
    }
    // No message is available but we're holding a session - let's suspend it.
    if (!tracker->session_id.has_value()) {
      tracker->session_id = tracker->stream.resumable_session_id();
      move(tracker->stream).Suspend();
    }
    // Schedule checking for a new message.
    auto schedule_result = io_async_executor_->ScheduleFor(
        bind(&GcpBlobStorageClientProvider::PutBlobStreamInternal, this,
             put_blob_stream_context, tracker),
        (TimeProvider::GetSteadyTimestampInNanoseconds() + kPutBlobRescanTime)
            .count());
    if (!schedule_result.Successful()) {
      put_blob_stream_context.result = schedule_result;
      SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                        put_blob_stream_context.result,
                        "Put blob stream request failed to be scheduled");
      FinishStreamingContext(schedule_result, put_blob_stream_context,
                             cpu_async_executor_);
    }
    return;
  }
  // Validate that the new request specifies the same blob.
  if (request->blob_portion().metadata().bucket_name() !=
          tracker->bucket_name ||
      request->blob_portion().metadata().blob_name() != tracker->blob_name) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      result,
                      "Enqueued message does not specify the same blob (bucket "
                      "name, blob name) as previously.");
    FinishStreamingContext(result, put_blob_stream_context,
                           cpu_async_executor_);
    return;
  }
  RestoreUploadIfSuspended(*tracker, cloud_storage_client);
  tracker->stream.write(request->blob_portion().data().c_str(),
                        request->blob_portion().data().length());
  if (!ValidateStream(put_blob_stream_context, tracker->stream).Successful()) {
    return;
  }
  // Schedule uploading the next portion.
  auto schedule_result = io_async_executor_->Schedule(
      bind(&GcpBlobStorageClientProvider::PutBlobStreamInternal, this,
           put_blob_stream_context, tracker),
      AsyncPriority::Normal);
  if (!schedule_result.Successful()) {
    put_blob_stream_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      put_blob_stream_context.result,
                      "Put blob stream request failed to be scheduled");
    FinishStreamingContext(schedule_result, put_blob_stream_context,
                           cpu_async_executor_);
  }
}

ExecutionResultOr<unique_ptr<ostream>>
GcpBlobStorageClientProvider::PutBlobStreamSync(
    const cmrt::sdk::blob_storage_service::v1::BlobIdentity&
        blob_identity) noexcept {
  if (blob_identity.blob_metadata().bucket_name().empty() ||
      blob_identity.blob_metadata().blob_name().empty()) {
    return FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
  }

  auto client_or =
      GetOrCreateCloudStroageClient(blob_identity.cloud_identity_info());
  if (!client_or.Successful()) {
    SCP_ERROR(kGcpBlobStorageClientProvider, kZeroUuid, client_or.result(),
              "Failed creating Google Cloud Storage client.");
    return client_or.result();
  }
  Client cloud_storage_client(*(client_or.value()));
  auto stream = make_unique<ObjectWriteStream>(cloud_storage_client.WriteObject(
      blob_identity.blob_metadata().bucket_name(),
      blob_identity.blob_metadata().blob_name()));

  return stream;
}

void GcpBlobStorageClientProvider::DeleteBlob(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
        delete_blob_context) noexcept {
  const auto& request = *delete_blob_context.request;
  if (request.blob_metadata().bucket_name().empty() ||
      request.blob_metadata().blob_name().empty()) {
    delete_blob_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, delete_blob_context,
        delete_blob_context.result,
        "Delete blob request failed. Missing bucket or blob name.");
    delete_blob_context.Finish();
    return;
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          bind(&GcpBlobStorageClientProvider::DeleteBlobInternal, this,
               delete_blob_context),
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    delete_blob_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, delete_blob_context,
                      delete_blob_context.result,
                      "Delete blob request failed to be scheduled");
    delete_blob_context.Finish();
  }
}

void GcpBlobStorageClientProvider::DeleteBlobInternal(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>
        delete_blob_context) noexcept {
  auto client_or = GetOrCreateCloudStroageClient(
      delete_blob_context.request->cloud_identity_info());
  if (!client_or.Successful()) {
    auto result = client_or.result();
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, delete_blob_context, result,
        "Create google cloud storage client failed for delete blob request.");
    FinishContext(result, delete_blob_context, cpu_async_executor_);
    return;
  }
  Client cloud_storage_client(*(client_or.value()));

  auto status = cloud_storage_client.DeleteObject(
      delete_blob_context.request->blob_metadata().bucket_name(),
      delete_blob_context.request->blob_metadata().blob_name());
  if (!status.ok()) {
    SCP_DEBUG_CONTEXT(kGcpBlobStorageClientProvider, delete_blob_context,
                      "Delete blob request failed. Error code: %d, message: %s",
                      status.code(), status.message().c_str());
    auto execution_result =
        GcpBlobStorageClientUtils::ConvertCloudStorageErrorToExecutionResult(
            status.code());
    FinishContext(execution_result, delete_blob_context, cpu_async_executor_);
    return;
  }
  delete_blob_context.response = make_shared<DeleteBlobResponse>();
  FinishContext(SuccessExecutionResult(), delete_blob_context,
                cpu_async_executor_);
}

ExecutionResultOr<shared_ptr<Client>>
GcpBlobStorageClientProvider::GetOrCreateCloudStroageClient(
    CloudIdentityInfo cloud_identity_info) noexcept {
  if (cloud_identity_info.owner_id().empty()) {
    cloud_identity_info.set_owner_id(current_project_id_);
  }
  string cached_key = SerializeDeterministically(cloud_identity_info);
  shared_ptr<Client> cached_client;
  auto execution_result =
      cloud_storage_client_pool_->Find(cached_key, cached_client);
  if (!execution_result.Successful()) {
    ExecutionResultOr<shared_ptr<Client>> client_or;
    ASSIGN_OR_LOG_AND_RETURN(client_or,
                             cloud_storage_factory_->CreateClient(
                                 options_, cloud_identity_info.owner_id(),
                                 cloud_identity_info.attestation_info()
                                     .gcp_attestation_info()
                                     .wip_provider()),
                             kGcpBlobStorageClientProvider, kZeroUuid,
                             "Failed creating Google Cloud Storage client.");

    auto key_value_pair = make_pair(cached_key, *client_or);
    execution_result =
        cloud_storage_client_pool_->Insert(key_value_pair, cached_client);
    if (!execution_result.Successful()) {
      SCP_ERROR(kGcpBlobStorageClientProvider, kZeroUuid, execution_result,
                "Failed adding Google Cloud Storage client to client pool.");
      return execution_result;
    }
  }
  return cached_client;
}

void GcpBlobStorageClientProvider::OnBeforeGarbageCollection(
    string& client_identity, shared_ptr<Client>& client,
    function<void(bool)> should_delete_entry) noexcept {
  should_delete_entry(true);
}

Options GcpCloudStorageFactory::CreateClientOptions(
    shared_ptr<BlobStorageClientOptions> options, const string& project_id,
    const std::string& wip_provider) noexcept {
  Options client_options;
  client_options.set<ProjectIdOption>(project_id);
  client_options.set<ConnectionPoolSizeOption>(kMaxConcurrentConnections);
  client_options.set<RetryPolicyOption>(
      LimitedErrorCountRetryPolicy(options->retry_limit).clone());
  client_options.set<IdempotencyPolicyOption>(
      StrictIdempotencyPolicy().clone());
  client_options.set<TransferStallTimeoutOption>(
      options->transfer_stall_timeout);
  if (!wip_provider.empty()) {
    string credentials_json = GcpUtils::CreateAttestedCredentials(wip_provider);
    client_options.set<UnifiedCredentialsOption>(
        MakeExternalAccountCredentials(credentials_json));
  }
  return client_options;
}

ExecutionResultOr<shared_ptr<Client>> GcpCloudStorageFactory::CreateClient(
    shared_ptr<BlobStorageClientOptions> options, const string& project_id,
    const string& wip_provider) noexcept {
  return make_shared<Client>(
      CreateClientOptions(options, project_id, wip_provider));
}

#ifndef TEST_CPIO
shared_ptr<BlobStorageClientProviderInterface>
BlobStorageClientProviderFactory::Create(
    shared_ptr<BlobStorageClientOptions> options,
    shared_ptr<InstanceClientProviderInterface> instance_client,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  return make_shared<GcpBlobStorageClientProvider>(
      options, instance_client, cpu_async_executor, io_async_executor);
}
#endif
}  // namespace google::scp::cpio::client_providers
