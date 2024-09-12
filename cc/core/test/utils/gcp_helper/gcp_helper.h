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

#pragma once

#include <memory>
#include <string>

#include <google/pubsub/v1/pubsub.grpc.pb.h>
#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>
#include <google/spanner/admin/instance/v1/spanner_instance_admin.grpc.pb.h>
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <google/storage/v2/storage.grpc.pb.h>

namespace google::scp::core::test {
/**
 * @brief Create a Publisher Stub object.
 *
 * @param endpoint the endpoint to the Publisher
 * @return std::unique_ptr<pubsub::v1::Publisher::StubInterface> the returned
 * Publisher stub.
 */
std::unique_ptr<pubsub::v1::Publisher::StubInterface> CreatePublisherStub(
    const std::string& endpoint);

/**
 * @brief Create a Topic object in the given Publisher.
 *
 * @param stub the given Publisher.
 * @param project_id the project ID for the topic.
 * @param topic_id the topic ID to be created.
 */
void CreateTopic(pubsub::v1::Publisher::StubInterface& stub,
                 const std::string& project_id, const std::string& topic_id);

/**
 * @brief Create a Subscriber Stub object.
 *
 * @param endpoint the endpoint to the Subscriber
 * @return std::unique_ptr<StubInterface> the returned
 * Subscriber stub.
 */
std::unique_ptr<pubsub::v1::Subscriber::StubInterface> CreateSubscriberStub(
    const std::string& endpoint);

/**
 * @brief Create a Subscription object in the given Subscriber.
 *
 * @param stub the given Subscriber.
 * @param project_id the project ID for the Subscription.
 * @param queue_name the Subscription ID and the Topic ID.
 */
void CreateSubscription(pubsub::v1::Subscriber::StubInterface& stub,
                        const std::string& project_id,
                        const std::string& queue_name);

/**
 * @brief Create a Spanner Stub object.
 *
 * @param endpoint the endpoint to the stub.
 * @return std::unique_ptr<StubInterface> the returned
 * Spanner stub.
 */
std::unique_ptr<spanner::v1::Spanner::StubInterface> CreateSpannerStub(
    const std::string& endpoint);

/**
 * @brief Create a Spanner InstanceAdmin Stub object.
 *
 * @param endpoint the endpoint to the stub.
 * @return std::unique_ptr<StubInterface> the
 * returned InstanceAdmin stub.
 */
std::unique_ptr<spanner::admin::instance::v1::InstanceAdmin::StubInterface>
CreateSpannerInstanceAdminStub(const std::string& endpoint);

/**
 * @brief Create a Spanner DatabaseAdmin Stub object.
 *
 * @param endpoint the endpoint to the stub.
 * @return std::unique_ptr<StubInterface> the
 * returned DatabaseAdmin stub.
 */
std::unique_ptr<spanner::admin::database::v1::DatabaseAdmin::StubInterface>
CreateSpannerDatabaseAdminStub(const std::string& endpoint);

/**
 * @brief Create a Spanner instance in the given Spanner.
 *
 * @param stub the given stub.
 * @param project_id the project ID.
 * @param instance_id the Spanner instance ID.
 */
void CreateSpannerInstance(
    spanner::admin::instance::v1::InstanceAdmin::StubInterface& stub,
    const std::string& project_id, const std::string& instance_id);

/**
 * @brief Create a Spanner database in the given Spanner.
 *
 * @param stub the given stub.
 * @param project_id the project ID.
 * @param instance_id the Spanner instance ID.
 * @param create_statement the statement to create the database.
 */
void CreateSpannerDatabase(
    spanner::admin::database::v1::DatabaseAdmin::StubInterface& stub,
    const std::string& project_id, const std::string& instance_id,
    const std::string& create_statement);

/**
 * @brief Create a Spanner table in the given Spanner database.
 *
 * @param stub the given stub.
 * @param project_id the project ID.
 * @param instance_id the Spanner instance ID.
 * @param database the Spanner database name.
 * @param create_statement the statement to create the table.
 */
void CreateSpannerTable(
    spanner::admin::database::v1::DatabaseAdmin::StubInterface& stub,
    const std::string& project_id, const std::string& instance_id,
    const std::string& database, const std::string& create_statement);

/**
 * @brief List sessions for the given Spanner database.
 *
 * @param stub the given stub.
 * @param project_id the project ID.
 * @param instance_id the Spanner instance ID.
 * @param database the Spanner database name.
 */
void ListSessions(spanner::v1::Spanner::StubInterface& stub,
                  const std::string& project_id, const std::string& instance_id,
                  const std::string& database);

/**
 * @brief Create a Storage Stub object.
 *
 * @param endpoint the endpoint to the Storage
 * @return std::unique_ptr<StubInterface> the returned
 * Storage stub.
 */
std::unique_ptr<storage::v2::Storage::StubInterface> CreateStorageStub(
    const std::string& endpoint);

/**
 * @brief Create storage bucket.
 *
 * @param stub the given stub.
 * @param project_id the project ID.
 * @param bucket_name the bucket name.
 */
void CreateBucket(storage::v2::Storage::StubInterface& stub,
                  const std::string& project_id,
                  const std::string& bucket_name);
}  // namespace google::scp::core::test
