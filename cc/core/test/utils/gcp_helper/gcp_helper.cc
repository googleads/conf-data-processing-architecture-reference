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

#include "gcp_helper.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include <google/pubsub/v1/pubsub.grpc.pb.h>
#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>
#include <google/spanner/admin/database/v1/spanner_database_admin.pb.h>
#include <google/spanner/admin/instance/v1/spanner_instance_admin.grpc.pb.h>
#include <google/spanner/admin/instance/v1/spanner_instance_admin.pb.h>
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <google/storage/v2/storage.grpc.pb.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"

using google::longrunning::Operation;
using google::pubsub::v1::Publisher;
using google::pubsub::v1::Subscriber;
using google::pubsub::v1::Subscription;
using google::pubsub::v1::Topic;
using google::spanner::admin::database::v1::CreateDatabaseRequest;
using google::spanner::admin::database::v1::DatabaseAdmin;
using google::spanner::admin::database::v1::UpdateDatabaseDdlRequest;
using google::spanner::admin::instance::v1::CreateInstanceRequest;
using google::spanner::admin::instance::v1::InstanceAdmin;
using google::spanner::v1::ListSessionsRequest;
using google::spanner::v1::ListSessionsResponse;
using google::spanner::v1::Spanner;
using google::storage::v2::Bucket;
using google::storage::v2::CreateBucketRequest;
using google::storage::v2::Storage;
using grpc::ClientContext;
using grpc::Status;
using grpc::StubOptions;
using std::make_shared;
using std::move;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::unique_ptr;

namespace google::scp::core::test {
unique_ptr<Publisher::StubInterface> CreatePublisherStub(
    const string& endpoint) {
  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  return Publisher::NewStub(channel, StubOptions());
}

unique_ptr<Subscriber::StubInterface> CreateSubscriberStub(
    const string& endpoint) {
  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  return Subscriber::NewStub(channel, StubOptions());
}

void CreateTopic(Publisher::StubInterface& stub, const string& project_id,
                 const string& topic_id) {
  auto topic_name = absl::StrCat("projects/", project_id, "/topics/", topic_id);
  Topic topic;
  topic.set_name(topic_name);
  ClientContext client_context;
  Topic response;
  auto status = stub.CreateTopic(&client_context, topic, &response);
  if (!status.ok()) {
    throw runtime_error("Failed to create topic:" + topic_name + ". " +
                        status.error_message());
  } else {
    std::cout << "Succeeded to create topic:" << topic_name << std::endl;
  }
}

void CreateSubscription(Subscriber::StubInterface& stub,
                        const string& project_id, const string& queue_name) {
  auto sub_name =
      absl::StrCat("projects/", project_id, "/subscriptions/", queue_name);
  Subscription sub;
  sub.set_name(sub_name);
  auto topic_name =
      absl::StrCat("projects/", project_id, "/topics/", queue_name);
  sub.set_topic(topic_name);
  ClientContext client_context;
  Subscription response;
  auto status = stub.CreateSubscription(&client_context, sub, &response);
  if (!status.ok()) {
    throw runtime_error("Failed to create subscription:" + sub_name + ". " +
                        status.error_message());
  } else {
    std::cout << "Succeeded to create subscription:" << sub_name << std::endl;
  }
}

unique_ptr<Spanner::StubInterface> CreateSpannerStub(const string& endpoint) {
  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  return Spanner::NewStub(channel, StubOptions());
}

unique_ptr<InstanceAdmin::StubInterface> CreateSpannerInstanceAdminStub(
    const string& endpoint) {
  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  return InstanceAdmin::NewStub(channel, StubOptions());
}

unique_ptr<DatabaseAdmin::StubInterface> CreateSpannerDatabaseAdminStub(
    const string& endpoint) {
  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  return DatabaseAdmin::NewStub(channel, StubOptions());
}

void ListSessions(Spanner::StubInterface& stub, const string& project_id,
                  const string& instance_id, const string& database) {
  ListSessionsRequest request;
  auto database_uri = absl::StrCat("projects/", project_id, "/instances/",
                                   instance_id, "/databases/", database);
  request.set_database(move(database_uri));
  ClientContext client_context;
  ListSessionsResponse response;
  auto status = stub.ListSessions(&client_context, request, &response);
  if (!status.ok()) {
    throw runtime_error("Failed to ListSessions: " + status.error_message());
  } else {
    std::cout << "Succeeded to ListSessions." << std::endl;
  }
}

void CreateSpannerInstance(InstanceAdmin::StubInterface& stub,
                           const string& project_id,
                           const string& instance_id) {
  CreateInstanceRequest request;
  request.set_parent(absl::StrCat("projects/", project_id));
  request.set_instance_id(instance_id);
  ClientContext client_context;
  Operation response;
  auto status = stub.CreateInstance(&client_context, request, &response);
  if (!status.ok()) {
    throw runtime_error("Failed to create Spanner instance: " + instance_id +
                        ". " + status.error_message());
  } else {
    std::cout << "Succeeded to create Spanner instance:" << instance_id
              << std::endl;
  }
}

void CreateSpannerDatabase(DatabaseAdmin::StubInterface& stub,
                           const string& project_id, const string& instance_id,
                           const string& create_statement) {
  auto parent =
      absl::StrCat("projects/", project_id, "/instances/", instance_id);
  CreateDatabaseRequest request;
  request.set_parent(parent);
  request.set_create_statement(create_statement);
  ClientContext client_context;
  Operation response;
  auto status = stub.CreateDatabase(&client_context, request, &response);
  if (!status.ok()) {
    throw runtime_error("Failed to create Spanner database: " + instance_id +
                        ". " + status.error_message());
  } else {
    std::cout << "Succeeded to create Spanner database:" << instance_id
              << std::endl;
  }
}

void CreateSpannerTable(DatabaseAdmin::StubInterface& stub,
                        const string& project_id, const string& instance_id,
                        const string& database,
                        const string& create_statement) {
  auto database_uri = absl::StrCat("projects/", project_id, "/instances/",
                                   instance_id, "/databases/", database);
  UpdateDatabaseDdlRequest request;
  request.set_database(database_uri);
  request.add_statements(create_statement);
  ClientContext client_context;
  Operation response;
  auto status = stub.UpdateDatabaseDdl(&client_context, request, &response);
  if (!status.ok()) {
    throw runtime_error("Failed to create Spanner table in database: " +
                        database + ". " + status.error_message());
  } else {
    std::cout << "Succeeded to create Spanner table in database: " << database
              << std::endl;
  }
}

unique_ptr<storage::v2::Storage::StubInterface> CreateStorageStub(
    const string& endpoint) {
  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  return Storage::NewStub(channel, StubOptions());
}

void CreateBucket(storage::v2::Storage::StubInterface& stub,
                  const string& project_id, const string& bucket_name) {
  CreateBucketRequest request;
  request.set_parent("projects/_");
  request.set_bucket_id(bucket_name);
  request.mutable_bucket()->set_project(absl::StrCat("projects/", project_id));
  request.mutable_bucket()->set_name(
      absl::StrCat("projects/_/buckets/", bucket_name));
  ClientContext client_context;
  Bucket response;
  auto status = stub.CreateBucket(&client_context, request, &response);
  if (!status.ok()) {
    throw runtime_error("Failed to create Storage bucket: " +
                        status.error_message());
  } else {
    std::cout << "Succeeded to create Storage bucket." << std::endl;
  }
}
}  // namespace google::scp::core::test
