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

#include "cpio/client_providers/auto_scaling_client_provider/src/gcp/gcp_auto_scaling_client_provider.h"

#include <gmock/gmock.h>

#include <memory>
#include <string>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/scp_test_base.h"
#include "cpio/client_providers/auto_scaling_client_provider/src/gcp/error_codes.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/error_codes.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_provider.h"
#include "cpio/client_providers/instance_database_client_provider/mock/mock_instance_database_client_provider.h"
#include "cpio/client_providers/instance_database_client_provider/src/common/error_codes.h"
#include "cpio/common/src/gcp/error_codes.h"
#include "google/cloud/compute/region_instance_group_managers/v1/mocks/mock_region_instance_group_managers_connection.h"
#include "operator/protos/shared/backend/asginstance/instance_status.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

using google::cloud::make_ready_future;
using google::cloud::Options;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::StatusOr;
using google::cloud::compute_region_instance_group_managers_v1::
    RegionInstanceGroupManagersClient;
using google::cloud::compute_region_instance_group_managers_v1_mocks::
    MockRegionInstanceGroupManagersConnection;
using google::cloud::cpp::compute::region_instance_group_managers::v1::
    DeleteInstancesRequest;
using google::cloud::cpp::compute::v1::Operation;
using google::cloud::cpp::compute::v1::
    RegionInstanceGroupManagersDeleteInstancesRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationResponse;
using google::cmrt::sdk::instance_database_client::GetInstanceByNameRequest;
using google::cmrt::sdk::instance_database_client::GetInstanceByNameResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::
    SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_GROUP_NAME_REQUIRED;
using google::scp::core::errors::
    SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::errors::SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE;
using google::scp::core::errors::SC_GCP_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::
    MockInstanceDatabaseClientProvider;
using google::scp::operators::protos::shared::backend::asginstance::
    InstanceStatus;
using std::atomic_bool;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::Return;

namespace {
constexpr char kInstanceResourceName[] =
    "//compute.googleapis.com/projects/123456/zones/us-central1-c/instances/"
    "1234567";
constexpr char kInstanceName[] =
    "https://www.googleapis.com/compute/v1/projects/123456/zones/"
    "us-central1-c/instances/1234567";
constexpr char kInstanceGroupName[] = "group_name";
}  // namespace

namespace google::scp::cpio::client_providers::test {

class MockInstanceGroupManagersClientFactory
    : public InstanceGroupManagersClientFactory {
 public:
  MOCK_METHOD(std::shared_ptr<RegionInstanceGroupManagersClient>, CreateClient,
              ((const shared_ptr<AutoScalingClientOptions>&)),
              (noexcept, override));
};

class MockInstanceDatabaseClientProviderFactory
    : public InstanceDatabaseClientProviderFactory {
 public:
  MOCK_METHOD(std::shared_ptr<InstanceDatabaseClientProviderInterface>,
              CreateClient,
              (const std::shared_ptr<AutoScalingClientOptions>&,
               const std::shared_ptr<InstanceClientProviderInterface>&,
               const std::shared_ptr<core::AsyncExecutorInterface>&,
               const std::shared_ptr<core::AsyncExecutorInterface>&),
              (noexcept, override));
};

class GcpAutoScalingClientProviderTest : public ScpTestBase {
 protected:
  GcpAutoScalingClientProviderTest() {
    mock_instance_client_ = make_shared<MockInstanceClientProvider>();
    mock_instance_client_->instance_resource_name = kInstanceResourceName;
    mock_instance_database_client_ =
        make_shared<NiceMock<MockInstanceDatabaseClientProvider>>();
    mock_instance_group_managers_client_factory_ =
        make_shared<NiceMock<MockInstanceGroupManagersClientFactory>>();
    connection_ =
        make_shared<NiceMock<MockRegionInstanceGroupManagersConnection>>();
    ON_CALL(*mock_instance_group_managers_client_factory_, CreateClient)
        .WillByDefault(Return(
            make_shared<RegionInstanceGroupManagersClient>(connection_)));
    mock_instance_database_client_factory_ =
        make_shared<NiceMock<MockInstanceDatabaseClientProviderFactory>>();
    ON_CALL(*mock_instance_database_client_factory_, CreateClient)
        .WillByDefault(Return(mock_instance_database_client_));

    auto_scaling_client_provider_ = make_unique<GcpAutoScalingClientProvider>(
        make_shared<AutoScalingClientOptions>(), mock_instance_client_,
        make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
        mock_instance_database_client_factory_,
        mock_instance_group_managers_client_factory_);
    try_termination_context_.request =
        make_shared<TryFinishInstanceTerminationRequest>();
    try_termination_context_.request->set_instance_resource_name(
        kInstanceResourceName);
    try_termination_context_.request->set_scale_in_hook_name(
        kInstanceGroupName);

    expected_delete_request_.set_project("123456");
    expected_delete_request_.set_region("us-central1");
    expected_delete_request_.set_instance_group_manager(kInstanceGroupName);

    *expected_delete_request_
         // The line length is too long.
         .mutable_region_instance_group_managers_delete_instances_request_resource()  // NOLINT
         ->add_instances() = kInstanceName;
  }

  void TearDown() override {
    EXPECT_SUCCESS(auto_scaling_client_provider_->Stop());
  }

  DeleteInstancesRequest expected_delete_request_;
  shared_ptr<MockInstanceClientProvider> mock_instance_client_;
  shared_ptr<MockInstanceDatabaseClientProviderFactory>
      mock_instance_database_client_factory_;
  shared_ptr<MockInstanceDatabaseClientProvider> mock_instance_database_client_;
  shared_ptr<MockRegionInstanceGroupManagersConnection> connection_;
  shared_ptr<MockInstanceGroupManagersClientFactory>
      mock_instance_group_managers_client_factory_;
  unique_ptr<GcpAutoScalingClientProvider> auto_scaling_client_provider_;

  AsyncContext<TryFinishInstanceTerminationRequest,
               TryFinishInstanceTerminationResponse>
      try_termination_context_;

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  atomic_bool finish_called_{false};
};

TEST_F(GcpAutoScalingClientProviderTest, MissingInstanceResourceName) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.request->clear_instance_resource_name();
  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        auto failure = FailureExecutionResult(
            SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED);
        EXPECT_THAT(context.result, ResultIs(failure));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName).Times(0);
  EXPECT_CALL(*connection_, DeleteInstances).Times(0);

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpAutoScalingClientProviderTest, InputInvalidInstanceResourceName) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.request->set_instance_resource_name("invalid");
  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        auto failure = FailureExecutionResult(
            SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME);
        EXPECT_THAT(context.result, ResultIs(failure));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName).Times(0);
  EXPECT_CALL(*connection_, DeleteInstances).Times(0);

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpAutoScalingClientProviderTest, MissingLifecycleHookName) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.request->clear_scale_in_hook_name();
  try_termination_context_
      .callback = [this](AsyncContext<TryFinishInstanceTerminationRequest,
                                      TryFinishInstanceTerminationResponse>&
                             context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(
            SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_GROUP_NAME_REQUIRED)));
    finish_called_ = true;
  };

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName).Times(0);
  EXPECT_CALL(*connection_, DeleteInstances).Times(0);

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpAutoScalingClientProviderTest, GetCurrentInstanceResourceNameFailed) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  mock_instance_client_->get_instance_resource_name_mock =
      FailureExecutionResult(SC_UNKNOWN);
  EXPECT_THAT(auto_scaling_client_provider_->Run(),
              ResultIs(mock_instance_client_->get_instance_resource_name_mock));
}

TEST_F(GcpAutoScalingClientProviderTest, ParseZoneFailed) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  mock_instance_client_->instance_resource_name =
      "//compute.googleapis.com/projects/123456/zones/us-central1/instances/"
      "1234567";
  EXPECT_THAT(auto_scaling_client_provider_->Run(),
              ResultIs(FailureExecutionResult(
                  SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE)));
}

TEST_F(GcpAutoScalingClientProviderTest, GetInstanceFailed) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName(_))
      .WillOnce([=](AsyncContext<GetInstanceByNameRequest,
                                 GetInstanceByNameResponse>& context) {
        EXPECT_EQ(context.request->instance_name(), kInstanceName);
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
      });
  EXPECT_CALL(*connection_, DeleteInstances).Times(0);

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpAutoScalingClientProviderTest, InstanceNotFoundInDatabase) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_FALSE(context.response->termination_scheduled());
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName(_))
      .WillOnce([=](AsyncContext<GetInstanceByNameRequest,
                                 GetInstanceByNameResponse>& context) {
        EXPECT_EQ(context.request->instance_name(), kInstanceName);
        context.result = FailureExecutionResult(
            SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND);
        context.Finish();
      });
  EXPECT_CALL(*connection_, DeleteInstances).Times(0);

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpAutoScalingClientProviderTest, InstanceAlreadyTerminated) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName(_))
      .WillOnce([=](AsyncContext<GetInstanceByNameRequest,
                                 GetInstanceByNameResponse>& context) {
        EXPECT_EQ(context.request->instance_name(), kInstanceName);
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetInstanceByNameResponse>();
        context.response->mutable_instance()->set_instance_name(kInstanceName);
        context.response->mutable_instance()->set_status(
            InstanceStatus::TERMINATED);
        context.Finish();
      });
  EXPECT_CALL(*connection_, DeleteInstances).Times(0);

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_FALSE(context.response->termination_scheduled());
        finish_called_ = true;
      };

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpAutoScalingClientProviderTest, NotInTerminatingWaitState) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName(_))
      .WillOnce([=](AsyncContext<GetInstanceByNameRequest,
                                 GetInstanceByNameResponse>& context) {
        EXPECT_EQ(context.request->instance_name(), kInstanceName);
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetInstanceByNameResponse>();
        context.response->mutable_instance()->set_instance_name(kInstanceName);
        context.response->mutable_instance()->set_status(
            InstanceStatus::UNKNOWN_INSTANCE_STATUS);
        context.Finish();
      });
  EXPECT_CALL(*connection_, DeleteInstances).Times(0);

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_FALSE(context.response->termination_scheduled());
        finish_called_ = true;
      };

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpAutoScalingClientProviderTest, TerminateInstanceFailed) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName(_))
      .WillOnce([=](AsyncContext<GetInstanceByNameRequest,
                                 GetInstanceByNameResponse>& context) {
        EXPECT_EQ(context.request->instance_name(), kInstanceName);
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetInstanceByNameResponse>();
        context.response->mutable_instance()->set_instance_name(kInstanceName);
        context.response->mutable_instance()->set_status(
            InstanceStatus::TERMINATING_WAIT);
        context.Finish();
      });
  EXPECT_CALL(*connection_, DeleteInstances)
      .WillOnce([&](DeleteInstancesRequest const& request) {
        EXPECT_THAT(request, EqualsProto(expected_delete_request_));
        return make_ready_future(
            StatusOr<Operation>(Status(StatusCode::kInternal, "")));
      });

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(SC_GCP_INTERNAL_SERVICE_ERROR)));
        finish_called_ = true;
      };

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpAutoScalingClientProviderTest, ScheduleTerminationSuccessfully) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  EXPECT_CALL(*mock_instance_database_client_, GetInstanceByName(_))
      .WillOnce([=](AsyncContext<GetInstanceByNameRequest,
                                 GetInstanceByNameResponse>& context) {
        EXPECT_EQ(context.request->instance_name(), kInstanceName);
        context.result = SuccessExecutionResult();
        context.response = make_shared<GetInstanceByNameResponse>();
        context.response->mutable_instance()->set_instance_name(kInstanceName);
        context.response->mutable_instance()->set_status(
            InstanceStatus::TERMINATING_WAIT);
        context.Finish();
      });
  EXPECT_CALL(*connection_, DeleteInstances)
      .WillOnce([&](DeleteInstancesRequest const& request) {
        EXPECT_THAT(request, EqualsProto(expected_delete_request_));
        Operation op;
        op.set_name("delete-instances");
        return make_ready_future(StatusOr<Operation>(op));
      });

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_TRUE(context.response->termination_scheduled());
        finish_called_ = true;
      };

  auto_scaling_client_provider_->TryFinishInstanceTermination(
      try_termination_context_);
  WaitUntil([this]() { return finish_called_.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
