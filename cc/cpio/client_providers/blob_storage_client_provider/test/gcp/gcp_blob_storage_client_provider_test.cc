// Copyright 2022 Google LLC
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

#include "cpio/client_providers/blob_storage_client_provider/src/gcp/gcp_blob_storage_client_provider.h"

#include <atomic>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/scp_test_base.h"
#include "core/utils/src/base64.h"
#include "core/utils/src/hashing.h"
#include "cpio/client_providers/blob_storage_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/blob_storage_client_provider/src/gcp/gcp_cloud_storage_client.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/object_write_streambuf.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::cloud::Status;
using google::cloud::StatusOr;
using CloudStatusCode = google::cloud::StatusCode;
using google::cloud::storage::Client;
using google::cloud::storage::DisableCrc32cChecksum;
using google::cloud::storage::DisableMD5Hash;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::ObjectWriteStream;
using google::cloud::storage::Prefix;
using google::cloud::storage::ReadRange;
using google::cloud::storage::StartOffset;
using google::cloud::storage::internal::CreateResumableUploadResponse;
using google::cloud::storage::internal::EmptyResponse;
using google::cloud::storage::internal::HttpResponse;
using google::cloud::storage::internal::InsertObjectMediaRequest;
using google::cloud::storage::internal::ListObjectsResponse;
using google::cloud::storage::internal::ObjectReadSource;
using google::cloud::storage::internal::ReadSourceResult;
using google::cloud::storage::internal::ResumableUploadRequest;
using google::cmrt::sdk::blob_storage_service::v1::Blob;
using google::cmrt::sdk::blob_storage_service::v1::BlobIdentity;
using google::cmrt::sdk::blob_storage_service::v1::BlobMetadata;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::common::v1::CloudIdentityInfo;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncOperation;
using google::scp::core::BytesBuffer;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_INVALID_CACHED_CLIENT_LIFETIME;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR;
using google::scp::core::errors::SC_GCP_DATA_LOSS;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::errors::SC_GCP_NOT_FOUND;
using google::scp::core::errors::SC_GCP_OUT_OF_RANGE;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::core::utils::CalculateMd5Hash;
using google::scp::cpio::BlobStorageClientOptions;
using google::scp::cpio::client_providers::GcpBlobStorageClientProvider;
using google::scp::cpio::client_providers::GcpCloudStorageFactory;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::get;
using std::make_shared;
using std::make_tuple;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::tuple;
using std::unique_ptr;
using std::vector;
using std::chrono::seconds;
using std::this_thread::sleep_for;
using testing::_;
using testing::ByMove;
using testing::ElementsAre;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::Pointwise;
using testing::Return;

namespace {
constexpr char kInstanceResourceName[] =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr char kBucketName1[] = "bucket1";
constexpr char kBucketName2[] = "bucket2";
constexpr char kBlobName1[] = "blob_1";
constexpr char kBlobName2[] = "blob_2";
constexpr char kOwnerId1[] = "testProjectId1";
constexpr char kOwnerId2[] = "testProjectId2";
constexpr char kWipProvider1[] = "testWipProvider1";
constexpr char kWipProvider2[] = "testWipProvider2";
constexpr seconds kCachedClientLifeTime = seconds(1);

constexpr uint64_t kDefaultMaxPageSize = 1000;
}  // namespace

#include "cpio/client_providers/blob_storage_client_provider/test/gcp/mock_gcp_cloud_storage_client.h"

using google::scp::cpio::client_providers::mock::BuildListObjectsReader;
using google::scp::cpio::client_providers::mock::
    BuildListObjectsReaderFromStatus;
using google::scp::cpio::client_providers::mock::BuildObjectWriteStream;
using google::scp::cpio::client_providers::mock::BuildReadStreamFromStatus;
using google::scp::cpio::client_providers::mock::BuildReadStreamFromString;
using google::scp::cpio::client_providers::mock::MockGcpCloudStorageClient;
using google::scp::cpio::client_providers::mock::MockGcpCloudStorageFactory;
using google::scp::cpio::client_providers::mock::MockObjectReadSource;

namespace google::scp::cpio::client_providers::test {

class GcpBlobStorageClientProviderBaseTest {
 protected:
  GcpBlobStorageClientProviderBaseTest()
      : options_(make_shared<BlobStorageClientOptions>()),
        instance_client_(make_shared<MockInstanceClientProvider>()),
        storage_factory_(make_shared<NiceMock<MockGcpCloudStorageFactory>>()),
        mock_gcs_client_(make_shared<NiceMock<MockGcpCloudStorageClient>>()),
        // We can't use mock executor for CPU calls because
        // AutoExpiryConcurrentMap in the GcpBlobStorageClient can only work
        // with real async executors.
        real_cpu_async_executor_(
            make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/1000)),
        gcp_blob_storage_client_(
            options_, instance_client_, real_cpu_async_executor_,
            make_shared<MockAsyncExecutor>(), storage_factory_) {
    ON_CALL(*storage_factory_, CreateClient)
        .WillByDefault(Return(mock_gcs_client_));
    options_->enable_new_gcp_error_code_converter = false;
    instance_client_->instance_resource_name = kInstanceResourceName;
    get_blob_context_.request = make_shared<GetBlobRequest>();
    get_blob_context_.callback = [this](auto) { finish_called_ = true; };

    list_blobs_context_.request = make_shared<ListBlobsMetadataRequest>();
    list_blobs_context_.callback = [this](auto) { finish_called_ = true; };

    put_blob_context_.request = make_shared<PutBlobRequest>();
    put_blob_context_.callback = [this](auto) { finish_called_ = true; };

    delete_blob_context_.request = make_shared<DeleteBlobRequest>();
    delete_blob_context_.callback = [this](auto) { finish_called_ = true; };

    EXPECT_SUCCESS(real_cpu_async_executor_->Init());
    EXPECT_SUCCESS(real_cpu_async_executor_->Run());

    EXPECT_SUCCESS(gcp_blob_storage_client_.Init());
    EXPECT_SUCCESS(gcp_blob_storage_client_.Run());
  }

  ~GcpBlobStorageClientProviderBaseTest() {
    EXPECT_SUCCESS(gcp_blob_storage_client_.Stop());
    EXPECT_SUCCESS(real_cpu_async_executor_->Stop());
  }

  shared_ptr<BlobStorageClientOptions> options_;
  shared_ptr<MockInstanceClientProvider> instance_client_;
  shared_ptr<MockGcpCloudStorageFactory> storage_factory_;
  shared_ptr<MockGcpCloudStorageClient> mock_gcs_client_;
  shared_ptr<AsyncExecutorInterface> real_cpu_async_executor_;
  GcpBlobStorageClientProvider gcp_blob_storage_client_;

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context_;

  AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
      list_blobs_context_;

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context_;

  BlobIdentity blob_identity_;

  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context_;
  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  std::atomic_bool finish_called_{false};
};

// Params:
// <begin_index, end_index, actual string to return, expected string to observe>
class GcpBlobStorageClientProviderTest
    : public ScpTestBase,
      public testing::WithParamInterface<
          tuple<uint64_t, uint64_t, string, string>>,
      public GcpBlobStorageClientProviderBaseTest {};

class GcpBlobStorageClientProviderWithAttestationTest
    : public ScpTestBase,
      public testing::WithParamInterface<tuple<string, string>>,
      public GcpBlobStorageClientProviderBaseTest {
 protected:
  template <typename Context>
  void MaybeExpectCreateClientCall(
      Context& context) {  // NOLINT(runtime/references)
    const auto& [owner_id, wip_provider] = GetParam();
    if (!owner_id.empty()) {
      context.request->mutable_cloud_identity_info()->set_owner_id(owner_id);
      context.request->mutable_cloud_identity_info()
          ->mutable_attestation_info()
          ->mutable_gcp_attestation_info()
          ->set_wip_provider(wip_provider);
      EXPECT_CALL(*storage_factory_, CreateClient(_, owner_id, wip_provider))
          .WillOnce(Return(mock_gcs_client_));
    }
  }
};

// Compares 2 BlobMetadata's bucket_name and blob_name.
MATCHER_P(BlobMetadataEquals, expected_metadata, "") {
  return ExplainMatchResult(arg.bucket_name(), expected_metadata.bucket_name(),
                            result_listener) &&
         ExplainMatchResult(arg.blob_name(), expected_metadata.blob_name(),
                            result_listener);
}

// Compares 2 Blobs, their metadata and data.
MATCHER_P(BlobEquals, expected_blob, "") {
  return ExplainMatchResult(BlobMetadataEquals(expected_blob.metadata()),
                            arg.metadata(), result_listener) &&
         ExplainMatchResult(expected_blob.data(), arg.data(), result_listener);
}

TEST_F(GcpBlobStorageClientProviderTest, InvalidCachedClientLifetime) {
  auto options = make_shared<BlobStorageClientOptions>();
  options->cached_client_lifetime = seconds(0);

  GcpBlobStorageClientProvider gcs_client(
      options, instance_client_, make_shared<MockAsyncExecutor>(),
      make_shared<MockAsyncExecutor>(), storage_factory_);

  EXPECT_THAT(gcs_client.Init(),
              ResultIs(FailureExecutionResult(
                  SC_BLOB_STORAGE_PROVIDER_INVALID_CACHED_CLIENT_LIFETIME)));
}

///////////// GetBlob /////////////////////////////////////////////////////////

// Builds an ObjectReadSource that contains the bytes (copied) from bytes_str.
StatusOr<unique_ptr<ObjectReadSource>> BuildReadResponseFromString(
    const string& bytes_str) {
  auto mock_source = make_unique<MockObjectReadSource>();
  auto offset = make_shared<size_t>(0);
  EXPECT_CALL(*mock_source, IsOpen)
      .WillRepeatedly(
          [offset, length = bytes_str.length()]() { return *offset < length; });
  EXPECT_CALL(*mock_source, Close)
      .WillRepeatedly(Return(HttpResponse{200, {}, {}}));
  EXPECT_CALL(*mock_source, Read)
      .WillRepeatedly(
          [bytes_str = bytes_str, offset](void* buf, std::size_t n) {
            std::multimap<string, string> headers = {
                {"x-goog-stored-content-length",
                 std::to_string(bytes_str.length())},
                {"content-length", std::to_string(bytes_str.length())}};
            if (*offset >= bytes_str.length()) {
              ReadSourceResult result{0, HttpResponse{200, {}, headers}};
              BytesBuffer buffer(bytes_str.length());
              buffer.bytes->assign(bytes_str.begin(), bytes_str.end());
              buffer.length = bytes_str.length();
              result.hashes.md5 = *CalculateMd5Hash(buffer);
              result.hashes.md5 = *Base64Encode(result.hashes.md5);
              result.size = bytes_str.length();
              return result;
            }
            auto length = std::min(bytes_str.length() - *offset, n);
            std::memcpy(buf, bytes_str.data() + *offset, length);
            *offset += length;
            ReadSourceResult result{length, HttpResponse{200, {}, headers}};
            BytesBuffer buffer(bytes_str.length());
            buffer.bytes->assign(bytes_str.begin(), bytes_str.end());
            buffer.length = bytes_str.length();
            result.hashes.md5 = *CalculateMd5Hash(buffer);
            result.hashes.md5 = *Base64Encode(result.hashes.md5);
            result.size = bytes_str.length();
            return result;
          });
  return unique_ptr<ObjectReadSource>(std::move(mock_source));
}

// Matches arg.bucket_name and arg.object_name with bucket_name and
// blob_name respectively. Also ensures that arg has DisableMD5Hash = false
// and DisableCrc32cChecksum = true.
MATCHER_P2(ReadObjectRequestEqual, bucket_name, blob_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(blob_name), arg.object_name(), result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<DisableMD5Hash>() ||
      arg.template GetOption<DisableMD5Hash>().value()) {
    *result_listener << "Expected ReadObjectRequest to have DisableMD5Hash == "
                        "false and it does not.";
    equal = false;
  }
  if (!arg.template HasOption<DisableCrc32cChecksum>() ||
      !arg.template GetOption<DisableCrc32cChecksum>().value()) {
    *result_listener << "Expected ReadObjectRequest to have "
                        "DisableCrc32cChecksum == true and it does not.";
    equal = false;
  }
  return equal;
}

MATCHER_P3(ReadCreateClientOptionsEqual, _, owner_id, wip_provider, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(owner_id), arg.project_id(), result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(wip_provider), arg.wip_provider(),
                          result_listener)) {
    equal = false;
  }
  return equal;
}

TEST_P(GcpBlobStorageClientProviderWithAttestationTest, GetBlob) {
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);

  MaybeExpectCreateClientCall(get_blob_context_);

  string bytes_str = "response_string";

  EXPECT_CALL(*mock_gcs_client_, ReadObject(kBucketName1, kBlobName1, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromString(bytes_str))));

  get_blob_context_.callback = [this, &bytes_str](auto& context) {
    EXPECT_SUCCESS(context.result);

    Blob expected_blob;
    expected_blob.mutable_metadata()->set_bucket_name(kBucketName1);
    expected_blob.mutable_metadata()->set_blob_name(kBlobName1);
    expected_blob.set_data(bytes_str);

    ASSERT_THAT(context.response, NotNull());
    EXPECT_THAT(context.response->blob(), BlobEquals(expected_blob));

    finish_called_ = true;
  };

  gcp_blob_storage_client_.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P4(ReadObjectRequestEqualsWithRange, bucket_name, blob_name,
           begin_index, end_index, "") {
  bool equal = true;
  if (!ExplainMatchResult(ReadObjectRequestEqual(bucket_name, blob_name), arg,
                          result_listener)) {
    equal = false;
  }

  if (!arg.template HasOption<ReadRange>() ||
      arg.template GetOption<ReadRange>().value().begin != begin_index ||
      arg.template GetOption<ReadRange>().value().end != end_index) {
    *result_listener << "Expected ReadObjectRequest to have ReadRange ("
                     << begin_index << ", " << end_index << ") but does not.";
    equal = false;
  }
  return equal;
}

TEST_P(GcpBlobStorageClientProviderTest, GetBlobWithByteRange) {
  const auto& [begin_index, end_index, actual_str, expected_str] = GetParam();
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);
  get_blob_context_.request->mutable_byte_range()->set_begin_byte_index(
      begin_index);
  get_blob_context_.request->mutable_byte_range()->set_end_byte_index(
      end_index);

  EXPECT_CALL(*mock_gcs_client_, ReadObject(kBucketName1, kBlobName1, _, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromString(actual_str))));

  get_blob_context_.callback = [this,
                                expected_str = expected_str](auto& context) {
    EXPECT_SUCCESS(context.result);

    Blob expected_blob;
    expected_blob.mutable_metadata()->set_bucket_name(kBucketName1);
    expected_blob.mutable_metadata()->set_blob_name(kBlobName1);
    expected_blob.set_data(expected_str);

    ASSERT_THAT(context.response, NotNull());
    EXPECT_THAT(context.response->blob(), BlobEquals(expected_blob));

    finish_called_ = true;
  };

  gcp_blob_storage_client_.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_P(GcpBlobStorageClientProviderWithAttestationTest, GetBlobStreamSync) {
  blob_identity_.mutable_blob_metadata()->set_bucket_name(kBucketName1);
  blob_identity_.mutable_blob_metadata()->set_blob_name(kBlobName1);
  blob_identity_.mutable_cloud_identity_info()->set_owner_id(kOwnerId1);
  blob_identity_.mutable_cloud_identity_info()
      ->mutable_attestation_info()
      ->mutable_gcp_attestation_info()
      ->set_wip_provider(kWipProvider1);

  string bytes_str = "response_string";

  EXPECT_CALL(*mock_gcs_client_, ReadObject(kBucketName1, kBlobName1, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromString(bytes_str))));
  auto result = gcp_blob_storage_client_.GetBlobStreamSync(blob_identity_);

  char output[256];
  result.value()->getline(output, 256);
  EXPECT_EQ(output, bytes_str);
  EXPECT_SUCCESS(result.result());
}

// Imagine the existing blob has data "0123456789". We exercise different cases
// for ranged reads on it.
// The tuples are <begin_index, end_index, string to return, expected string>
// We pad 'a' to the string to return so that the content length is always 10.
INSTANTIATE_TEST_SUITE_P(
    ByteRangeTest, GcpBlobStorageClientProviderTest,
    testing::Values(
        // Range is full length of object.
        make_tuple(0, 9, "0123456789", "0123456789"),
        // Range starts at offset.
        make_tuple(2, 9, "23456789aa", "23456789"),
        // Range ends at offset.
        make_tuple(0, 7, "01234567aa", "01234567"),
        // Range is a shifted window - "aa" should be ignored.
        make_tuple(2, 11, "23456789aa", "23456789"),
        // Range is longer than object length - "aa" should be ignored
        make_tuple(2, 15, "23456789aa", "23456789")));

INSTANTIATE_TEST_SUITE_P(AttestationTest,
                         GcpBlobStorageClientProviderWithAttestationTest,
                         testing::Values(
                             // No attestation provded.
                             make_tuple("", ""),
                             // With attestation.
                             make_tuple(kOwnerId1, kWipProvider1)));

StatusOr<unique_ptr<ObjectReadSource>> BuildBadHashReadResponse() {
  auto mock_source = make_unique<MockObjectReadSource>();
  auto offset = make_shared<size_t>(0);
  string bytes_str = "0123456789";
  EXPECT_CALL(*mock_source, IsOpen)
      .WillRepeatedly(
          [offset, length = bytes_str.length()]() { return *offset < length; });
  EXPECT_CALL(*mock_source, Close)
      .WillRepeatedly(Return(HttpResponse{200, {}, {}}));
  EXPECT_CALL(*mock_source, Read)
      .WillRepeatedly([bytes_str, offset](void* buf, std::size_t n) {
        std::multimap<string, string> headers = {
            {"x-goog-stored-content-length",
             std::to_string(bytes_str.length())},
            {"content-length", std::to_string(bytes_str.length())}};
        if (*offset >= bytes_str.length()) {
          ReadSourceResult result{0, HttpResponse{200, {}, headers}};
          result.hashes.md5 = "1B2M2Y8AsgTpgAmY7PhCfg==";
          result.size = bytes_str.length();
          return result;
        }
        auto length = std::min(bytes_str.length() - *offset, n);
        std::memcpy(buf, bytes_str.data() + *offset, length);
        *offset += length;
        ReadSourceResult result{length, HttpResponse{200, {}, headers}};
        result.hashes.md5 = "1B2M2Y8AsgTpgAmY7PhCfg==";
        result.size = bytes_str.length();
        return result;
      });
  return unique_ptr<ObjectReadSource>(std::move(mock_source));
}

google::cloud::storage::ObjectReadStream BuildBadHashReadStream() {
  auto source = BuildBadHashReadResponse();
  google::cloud::storage::internal::ReadObjectRangeRequest request;
  request.set_option(google::cloud::storage::DisableCrc32cChecksum(true));
  request.set_option(google::cloud::storage::DisableMD5Hash(false));
  return google::cloud::storage::ObjectReadStream(
      make_unique<google::cloud::storage::internal::ObjectReadStreambuf>(
          std::move(request), std::move(*source)));
}

TEST_F(GcpBlobStorageClientProviderTest, GetBlobHashMismatchFails) {
  options_->enable_new_gcp_error_code_converter = true;
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);

  EXPECT_CALL(*mock_gcs_client_, ReadObject(kBucketName1, kBlobName1, _, _))
      .WillOnce(Return(ByMove(BuildBadHashReadStream())));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_DATA_LOSS)));
    EXPECT_THAT(context.response, IsNull());

    finish_called_ = true;
  };

  gcp_blob_storage_client_.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest, GetBlobNotFound) {
  options_->enable_new_gcp_error_code_converter = true;
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);

  EXPECT_CALL(*mock_gcs_client_, ReadObject(kBucketName1, kBlobName1, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromStatus(
          Status(CloudStatusCode::kNotFound, "Blob not found")))));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_NOT_FOUND)));
    EXPECT_THAT(context.response, IsNull());

    finish_called_ = true;
  };

  gcp_blob_storage_client_.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

///////////// ListBlobs ///////////////////////////////////////////////////////

// Matches a ListObjectsRequest with bucket_name and no Prefix.
// Ensures that MaxResults is present and is 1000.
// Ensures StartOffset is not present.
MATCHER_P(ListObjectsRequestEqualNoOffset, bucket_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template GetOption<Prefix>().has_value()) {
    *result_listener
        << "Expected arg to not have a present Prefix value but has: "
        << arg.template GetOption<Prefix>().value();
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(1000),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template HasOption<StartOffset>()) {
    if (auto offset = arg.template GetOption<StartOffset>();
        !offset.value().empty()) {
      *result_listener
          << "Expected ListObjectsRequest to not have StartOffset but has: "
          << offset.value();
      equal = false;
    }
  }
  return equal;
}

TEST_P(GcpBlobStorageClientProviderWithAttestationTest, ListBlobsNoPrefix) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);

  MaybeExpectCreateClientCall(list_blobs_context_);

  EXPECT_CALL(*mock_gcs_client_, ListObjects(kBucketName1, _, _))
      .WillOnce(Return(
          ByMove(BuildListObjectsReader(ListObjectsResponse::FromHttpResponse(
              absl::StrFormat(R"""({
            "items": [
              {
                "name": "%s"
              },
              {
                "name": "%s"
              }
            ]
          })""",
                              kBlobName1, kBlobName2))))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());

    BlobMetadata expected_metadata1, expected_metadata2;
    expected_metadata1.set_bucket_name(kBucketName1);
    expected_metadata1.set_blob_name(kBlobName1);
    expected_metadata2.set_bucket_name(kBucketName1);
    expected_metadata2.set_blob_name(kBlobName2);

    EXPECT_THAT(context.response->blob_metadatas(),
                ElementsAre(BlobMetadataEquals(expected_metadata1),
                            BlobMetadataEquals(expected_metadata2)));
    EXPECT_FALSE(context.response->has_next_page_token());

    finish_called_ = true;
  };

  gcp_blob_storage_client_.ListBlobsMetadata(list_blobs_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

// Matches a ListObjectsRequest with bucket_name and Prefix(blob_name).
// Ensures that MaxResults is present and is max_results.
// Ensures StartOffset is not present.
MATCHER_P3(ListObjectsRequestEqualNoOffset, bucket_name, blob_name, max_results,
           "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<Prefix>() ||
      !ExplainMatchResult(Eq(blob_name),
                          arg.template GetOption<Prefix>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(max_results),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template HasOption<StartOffset>()) {
    if (auto offset = arg.template GetOption<StartOffset>();
        !offset.value().empty()) {
      *result_listener
          << "Expected ListObjectsRequest to not have StartOffset but has: "
          << offset.value();
      equal = false;
    }
  }
  return equal;
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsWithPrefix) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");

  EXPECT_CALL(*mock_gcs_client_, ListObjects(kBucketName1, _, _))
      .WillOnce(Return(
          ByMove(BuildListObjectsReader(ListObjectsResponse::FromHttpResponse(
              absl::StrFormat(R"""({
            "items": [
              {
                "name": "%s"
              },
              {
                "name": "%s"
              }
            ]
          })""",
                              kBlobName1, kBlobName2))))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());

    BlobMetadata expected_metadata1, expected_metadata2;
    expected_metadata1.set_bucket_name(kBucketName1);
    expected_metadata1.set_blob_name(kBlobName1);
    expected_metadata2.set_bucket_name(kBucketName1);
    expected_metadata2.set_blob_name(kBlobName2);

    EXPECT_THAT(context.response->blob_metadatas(),
                ElementsAre(BlobMetadataEquals(expected_metadata1),
                            BlobMetadataEquals(expected_metadata2)));
    EXPECT_FALSE(context.response->has_next_page_token());

    finish_called_ = true;
  };

  gcp_blob_storage_client_.ListBlobsMetadata(list_blobs_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

// Matches a ListObjectsRequest with bucket_name and blob_name.
// Ensures that MaxResults is present and is max_results.
// Ensures StartOffset is present and is offset.
MATCHER_P4(ListObjectsRequestEqualWithOffset, bucket_name, blob_name,
           max_results, offset, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<Prefix>() ||
      !ExplainMatchResult(Eq(blob_name),
                          arg.template GetOption<Prefix>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(max_results),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<StartOffset>() ||
      !ExplainMatchResult(Eq(offset),
                          arg.template GetOption<StartOffset>().value(),
                          result_listener)) {
    equal = false;
  }
  return equal;
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsWithMarker) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");
  list_blobs_context_.request->set_page_token(kBlobName1);

  EXPECT_CALL(*mock_gcs_client_, ListObjects(kBucketName1, _, _, _))
      .WillOnce(Return(
          ByMove(BuildListObjectsReader(ListObjectsResponse::FromHttpResponse(
              absl::StrFormat(R"""({
            "items": [
              {
                "name": "%s"
              }
            ]
          })""",
                              kBlobName2))))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());

    BlobMetadata expected_metadata;
    expected_metadata.set_bucket_name(kBucketName1);
    expected_metadata.set_blob_name(kBlobName2);

    EXPECT_THAT(context.response->blob_metadatas(),
                ElementsAre(BlobMetadataEquals(expected_metadata)));
    EXPECT_FALSE(context.response->has_next_page_token());

    finish_called_ = true;
  };

  gcp_blob_storage_client_.ListBlobsMetadata(list_blobs_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsWithMarkerSkipsFirstObject) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");
  list_blobs_context_.request->set_page_token(kBlobName1);

  EXPECT_CALL(*mock_gcs_client_, ListObjects(kBucketName1, _, _, _))
      .WillOnce(Return(
          ByMove(BuildListObjectsReader(ListObjectsResponse::FromHttpResponse(
              absl::StrFormat(R"""({
            "items": [
              {
                "name": "%s"
              },
              {
                "name": "%s"
              }
            ]
          })""",
                              kBlobName1, kBlobName2))))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());

    BlobMetadata expected_metadata;
    expected_metadata.set_bucket_name(kBucketName1);
    expected_metadata.set_blob_name(kBlobName2);

    EXPECT_THAT(context.response->blob_metadatas(),
                ElementsAre(BlobMetadataEquals(expected_metadata)));
    EXPECT_FALSE(context.response->has_next_page_token());

    finish_called_ = true;
  };

  gcp_blob_storage_client_.ListBlobsMetadata(list_blobs_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

// Used for Pointwise matching of Blob Metadata -> Blob Metadatas using the
// MATCHER_P version.
MATCHER(BlobMetadatasEqual, "") {
  const auto& actual_metadata = std::get<0>(arg);
  const auto& expected_metadata = std::get<1>(arg);
  return ExplainMatchResult(BlobMetadataEquals(expected_metadata),
                            actual_metadata, result_listener);
}

TEST_F(GcpBlobStorageClientProviderTest,
       ListBlobsReturnsMarkerAndEnforcesPageSize) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");

  const auto page_size = 100;
  list_blobs_context_.request->set_max_page_size(page_size);

  // Make a JSON object with items named 1 to page_size + 5.
  string items_str;
  for (int64_t i = 1; i <= page_size + 5; i++) {
    if (!items_str.empty()) {
      absl::StrAppend(&items_str, ",");
    }
    absl::StrAppendFormat(&items_str, R"""({"name": "%s"})""",
                          absl::StrCat("blob_", i));
  }

  EXPECT_CALL(*mock_gcs_client_, ListObjects(kBucketName1, _, _))
      .WillOnce(Return(
          ByMove(BuildListObjectsReader(ListObjectsResponse::FromHttpResponse(
              absl::StrFormat(R"""({"items": [%s]})""", items_str))))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    ASSERT_THAT(context.response, NotNull());

    // We expect to only see blobs 1-100, not [101, 105].
    std::vector<BlobMetadata> expected_blobs;
    expected_blobs.reserve(page_size);
    for (int64_t i = 1; i <= page_size; i++) {
      BlobMetadata metadata;
      metadata.set_bucket_name(kBucketName1);
      metadata.set_blob_name(absl::StrCat("blob_", i));
      expected_blobs.push_back(std::move(metadata));
    }
    EXPECT_THAT(context.response->blob_metadatas(),
                Pointwise(BlobMetadatasEqual(), expected_blobs));
    EXPECT_THAT(context.response->next_page_token(), "blob_100");

    finish_called_ = true;
  };

  gcp_blob_storage_client_.ListBlobsMetadata(list_blobs_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsPropagatesFailure) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");

  EXPECT_CALL(*mock_gcs_client_, ListObjects(kBucketName1, _, _))
      .WillOnce(Return(ByMove(BuildListObjectsReaderFromStatus(
          Status(CloudStatusCode::kInvalidArgument, "error")))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));

    finish_called_ = true;
  };

  gcp_blob_storage_client_.ListBlobsMetadata(list_blobs_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest,
       ListBlobsPropagatesFailureReturnGcpErrorCode) {
  options_->enable_new_gcp_error_code_converter = true;
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");

  EXPECT_CALL(*mock_gcs_client_, ListObjects(kBucketName1, _, _))
      .WillOnce(Return(ByMove(BuildListObjectsReaderFromStatus(
          Status(CloudStatusCode::kInvalidArgument, "error")))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_INVALID_ARGUMENT)));

    finish_called_ = true;
  };

  gcp_blob_storage_client_.ListBlobsMetadata(list_blobs_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

///////////// PutBlob /////////////////////////////////////////////////////////

MATCHER_P(InsertObjectRequestEquals, expected_request, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(expected_request.bucket_name()), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(expected_request.object_name()), arg.object_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(expected_request.contents()), arg.contents(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MD5HashValue>() ||
      !ExplainMatchResult(
          Eq(expected_request.template GetOption<MD5HashValue>().value()),
          arg.template GetOption<MD5HashValue>().value(), result_listener)) {
    *result_listener << "Expected arg has the same MD5 but does not.";
    equal = false;
  }
  return equal;
}

TEST_P(GcpBlobStorageClientProviderWithAttestationTest, PutBlob) {
  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName1);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      kBlobName1);

  MaybeExpectCreateClientCall(put_blob_context_);

  string bytes_str = "put_string";
  put_blob_context_.request->mutable_blob()->set_data(bytes_str);

  // Use Google Cloud's MD5 method.
  string expected_md5_hash = google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(kBucketName1, kBlobName1,
                                            bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_gcs_client_,
              InsertObject(kBucketName1, kBlobName1, bytes_str, _))
      .WillOnce(Return(ObjectMetadata()));

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  gcp_blob_storage_client_.PutBlob(put_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_P(GcpBlobStorageClientProviderWithAttestationTest, PutBlobStreamSync) {
  blob_identity_.mutable_blob_metadata()->set_bucket_name(kBucketName1);
  blob_identity_.mutable_blob_metadata()->set_blob_name(kBlobName1);

  EXPECT_CALL(*mock_gcs_client_, WriteObject(kBucketName1, kBlobName1))
      .WillOnce(Return(ByMove(BuildObjectWriteStream())));
  auto result = gcp_blob_storage_client_.PutBlobStreamSync(blob_identity_);
  EXPECT_SUCCESS(result.result());
  result.value()->write("test_string", 128);
  EXPECT_FALSE(result.value()->bad());
}

TEST_F(GcpBlobStorageClientProviderTest, PutBlobPropagatesFailure) {
  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName1);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      kBlobName1);

  string bytes_str = "put_string";
  put_blob_context_.request->mutable_blob()->set_data(bytes_str);

  // Use Google Cloud's MD5 method.
  string expected_md5_hash = google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(kBucketName1, kBlobName1,
                                            bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_gcs_client_,
              InsertObject(kBucketName1, kBlobName1, bytes_str, _))
      .WillOnce(Return(Status(CloudStatusCode::kOutOfRange, "failure")));

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB)));

    finish_called_ = true;
  };

  gcp_blob_storage_client_.PutBlob(put_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest,
       PutBlobPropagatesFailureReturnsGcpErrorCode) {
  options_->enable_new_gcp_error_code_converter = true;
  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName1);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      kBlobName1);

  string bytes_str = "put_string";
  put_blob_context_.request->mutable_blob()->set_data(bytes_str);

  // Use Google Cloud's MD5 method.
  string expected_md5_hash = google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(kBucketName1, kBlobName1,
                                            bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_gcs_client_,
              InsertObject(kBucketName1, kBlobName1, bytes_str, _))
      .WillOnce(Return(Status(CloudStatusCode::kOutOfRange, "failure")));

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_OUT_OF_RANGE)));

    finish_called_ = true;
  };

  gcp_blob_storage_client_.PutBlob(put_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

///////////// DeleteBlob //////////////////////////////////////////////////////

MATCHER_P2(DeleteObjectRequestEquals, bucket_name, blob_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(blob_name), arg.object_name(), result_listener)) {
    equal = false;
  }
  return equal;
}

TEST_P(GcpBlobStorageClientProviderWithAttestationTest, DeleteBlob) {
  delete_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  delete_blob_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName1);

  MaybeExpectCreateClientCall(delete_blob_context_);

  EXPECT_CALL(*mock_gcs_client_, DeleteObject(kBucketName1, kBlobName1))
      .WillOnce(Return(Status()));

  delete_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    finish_called_ = true;
  };

  gcp_blob_storage_client_.DeleteBlob(delete_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest,
       OperationsWithMissingAttestationsUseDefaultGCSClient) {
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId1, "")).Times(0);

  string bytes_str = "response_string";

  EXPECT_CALL(*mock_gcs_client_, ReadObject(kBucketName1, kBlobName1, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromString(bytes_str))));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);
    finish_called_ = true;
  };

  gcp_blob_storage_client_.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });

  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName1);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      kBlobName1);
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId1, "")).Times(0);

  put_blob_context_.request->mutable_blob()->set_data(bytes_str);

  string expected_md5_hash = google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(kBucketName1, kBlobName1,
                                            bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_gcs_client_,
              InsertObject(kBucketName1, kBlobName1, bytes_str, _))
      .WillOnce(Return(ObjectMetadata()));

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);
    finish_called_ = true;
  };

  gcp_blob_storage_client_.PutBlob(put_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest,
       MulitpleOperationsWithDifferentAttenstations) {
  auto mock_client_with_attestation_1 =
      make_shared<NiceMock<MockGcpCloudStorageClient>>();
  auto mock_client_with_attestation_2 =
      make_shared<NiceMock<MockGcpCloudStorageClient>>();

  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);
  get_blob_context_.request->mutable_cloud_identity_info()->set_owner_id(
      kOwnerId1);
  get_blob_context_.request->mutable_cloud_identity_info()
      ->mutable_attestation_info()
      ->mutable_gcp_attestation_info()
      ->set_wip_provider(kWipProvider1);
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId1, kWipProvider1))
      .WillOnce(Return(mock_client_with_attestation_1));
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId2, kWipProvider2))
      .Times(0);

  string bytes_str = "response_string";

  EXPECT_CALL(*mock_client_with_attestation_1,
              ReadObject(kBucketName1, kBlobName1, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromString(bytes_str))));
  EXPECT_CALL(*mock_client_with_attestation_2,
              ReadObject(kBucketName1, kBlobName1, _, _))
      .Times(0);

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);
    finish_called_ = true;
  };

  gcp_blob_storage_client_.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });

  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName2);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName2);
  get_blob_context_.request->mutable_cloud_identity_info()->set_owner_id(
      kOwnerId2);
  get_blob_context_.request->mutable_cloud_identity_info()
      ->mutable_attestation_info()
      ->mutable_gcp_attestation_info()
      ->set_wip_provider(kWipProvider2);
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId2, kWipProvider2))
      .WillOnce(Return(mock_client_with_attestation_2));
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId1, kWipProvider1))
      .Times(0);

  EXPECT_CALL(*mock_client_with_attestation_2,
              ReadObject(kBucketName2, kBlobName2, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromString(bytes_str))));
  EXPECT_CALL(*mock_client_with_attestation_1,
              ReadObject(kBucketName2, kBlobName2, _, _))
      .Times(0);

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);
    finish_called_ = true;
  };

  gcp_blob_storage_client_.GetBlob(get_blob_context_);

  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName1);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      kBlobName1);
  put_blob_context_.request->mutable_cloud_identity_info()->set_owner_id(
      kOwnerId1);
  put_blob_context_.request->mutable_cloud_identity_info()
      ->mutable_attestation_info()
      ->mutable_gcp_attestation_info()
      ->set_wip_provider(kWipProvider1);
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId1, kWipProvider1))
      .Times(0);
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId2, kWipProvider2))
      .Times(0);

  put_blob_context_.request->mutable_blob()->set_data(bytes_str);

  string expected_md5_hash = google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(kBucketName1, kBlobName1,
                                            bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_client_with_attestation_1,
              InsertObject(kBucketName1, kBlobName1, bytes_str, _))
      .WillOnce(Return(ObjectMetadata()));
  EXPECT_CALL(*mock_client_with_attestation_2,
              InsertObject(kBucketName1, kBlobName1, bytes_str, _))
      .Times(0);

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);
    finish_called_ = true;
  };

  gcp_blob_storage_client_.PutBlob(put_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest, CreateGCSClientFailed) {
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);
  get_blob_context_.request->mutable_cloud_identity_info()->set_owner_id(
      kOwnerId1);
  get_blob_context_.request->mutable_cloud_identity_info()
      ->mutable_attestation_info()
      ->mutable_gcp_attestation_info()
      ->set_wip_provider(kWipProvider1);
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId1, kWipProvider1))
      .WillOnce(Return(
          FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS)));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(
                                    SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS)));
    finish_called_ = true;
  };

  gcp_blob_storage_client_.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpBlobStorageClientProviderTest,
       MulitpleOperationsWithGCSClientRecreation) {
  auto cpu_async_executor =
      make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/1000);
  auto options = make_shared<BlobStorageClientOptions>();
  options->cached_client_lifetime = kCachedClientLifeTime;

  // We can't use mock executor for CPU calls because AutoExpiryConcurrentMap in
  // the GcpBlobStorageClient can only work with real async executors.
  GcpBlobStorageClientProvider gcs_client(
      options, instance_client_, cpu_async_executor,
      make_shared<MockAsyncExecutor>(), storage_factory_);
  ON_CALL(*storage_factory_, CreateClient)
      .WillByDefault(Return(mock_gcs_client_));

  EXPECT_SUCCESS(cpu_async_executor->Init());
  EXPECT_SUCCESS(cpu_async_executor->Run());

  EXPECT_SUCCESS(gcs_client.Init());
  EXPECT_SUCCESS(gcs_client.Run());

  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName1);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);
  get_blob_context_.request->mutable_cloud_identity_info()->set_owner_id(
      kOwnerId1);
  get_blob_context_.request->mutable_cloud_identity_info()
      ->mutable_attestation_info()
      ->mutable_gcp_attestation_info()
      ->set_wip_provider(kWipProvider1);

  auto mock_client_with_attestation =
      make_shared<NiceMock<MockGcpCloudStorageClient>>();
  EXPECT_CALL(*storage_factory_, CreateClient(_, kOwnerId1, kWipProvider1))
      .Times(2)
      .WillRepeatedly(Return(mock_client_with_attestation));

  string bytes_str = "response_string";
  EXPECT_CALL(*mock_client_with_attestation,
              ReadObject(kBucketName1, kBlobName1, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromString(bytes_str))));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);
    finish_called_ = true;
  };

  gcs_client.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });

  // Wait until the the client in the client pool expires.
  sleep_for(seconds(3));

  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName2);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName2);

  EXPECT_CALL(*mock_client_with_attestation,
              ReadObject(kBucketName2, kBlobName2, _, _))
      .WillOnce(Return(ByMove(BuildReadStreamFromString(bytes_str))));

  gcs_client.GetBlob(get_blob_context_);

  WaitUntil([this]() { return finish_called_.load(); });

  EXPECT_SUCCESS(gcs_client.Stop());
  EXPECT_SUCCESS(cpu_async_executor->Stop());
}

}  // namespace google::scp::cpio::client_providers::test
