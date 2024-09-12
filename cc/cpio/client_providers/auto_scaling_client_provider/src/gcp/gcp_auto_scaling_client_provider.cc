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

#include "gcp_auto_scaling_client_provider.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"
#include "cpio/client_providers/instance_database_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/instance_database_client_provider/src/gcp/gcp_instance_database_client_provider.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_database_client_provider_interface.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "google/cloud/compute/region_instance_group_managers/v1/region_instance_group_managers_client.h"
#include "google/cloud/compute/region_instance_group_managers/v1/region_instance_group_managers_rest_connection.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "operator/protos/shared/backend/asginstance/instance_status.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

#include "error_codes.h"

using google::cloud::future;
using google::cloud::Options;
using google::cloud::StatusOr;
using google::cloud::compute_region_instance_group_managers_v1::
    MakeRegionInstanceGroupManagersConnectionRest;
using google::cloud::compute_region_instance_group_managers_v1::
    RegionInstanceGroupManagersClient;
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
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_GROUP_NAME_REQUIRED;
using google::scp::core::errors::
    SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED;
using google::scp::core::errors::
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::operators::protos::shared::backend::asginstance::
    InstanceStatus;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace {
constexpr char kGcpAutoScalingClientProvider[] = "GcpAutoScalingClientProvider";
}

namespace google::scp::cpio::client_providers {
ExecutionResult GcpAutoScalingClientProvider::Init() noexcept {
  instance_database_client_provider_ =
      instance_database_client_provider_factory_->CreateClient(
          options_, instance_client_provider_, cpu_async_executor_,
          io_async_executor_);
  return instance_database_client_provider_->Init();
}

ExecutionResult GcpAutoScalingClientProvider::Run() noexcept {
  string current_instance_resource_name;
  RETURN_AND_LOG_IF_FAILURE(
      instance_client_provider_->GetCurrentInstanceResourceNameSync(
          current_instance_resource_name),
      kGcpAutoScalingClientProvider, kZeroUuid,
      "Failed to get current instance resource name.");
  auto current_zone_or =
      GcpInstanceClientUtils::ParseZoneIdFromInstanceResourceName(
          current_instance_resource_name);
  RETURN_AND_LOG_IF_FAILURE(current_zone_or.result(),
                            kGcpAutoScalingClientProvider, kZeroUuid,
                            "Failed to parse current instance zone ID.");
  auto current_region_or =
      GcpInstanceClientUtils::ExtractRegion(*current_zone_or);
  RETURN_AND_LOG_IF_FAILURE(current_region_or.result(),
                            kGcpAutoScalingClientProvider, kZeroUuid,
                            "Failed to extract current instance region ID.");
  current_region_ = move(*current_region_or);
  auto current_project_id_or_ =
      GcpInstanceClientUtils::ParseProjectIdFromInstanceResourceName(
          current_instance_resource_name);
  RETURN_AND_LOG_IF_FAILURE(current_project_id_or_.result(),
                            kGcpAutoScalingClientProvider, kZeroUuid,
                            "Failed to parse current project ID.");
  current_project_id_ = move(*current_project_id_or_);

  instance_group_managers_client_ =
      instance_group_managers_client_factory_->CreateClient(options_);

  return instance_database_client_provider_->Run();
}

ExecutionResult GcpAutoScalingClientProvider::Stop() noexcept {
  return instance_database_client_provider_->Stop();
}

void GcpAutoScalingClientProvider::TryFinishInstanceTermination(
    AsyncContext<TryFinishInstanceTerminationRequest,
                 TryFinishInstanceTerminationResponse>&
        try_termination_context) noexcept {
  if (try_termination_context.request->instance_resource_name().empty()) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED);
    SCP_ERROR_CONTEXT(kGcpAutoScalingClientProvider, try_termination_context,
                      execution_result, "Invalid request.");
    try_termination_context.result = execution_result;
    try_termination_context.Finish();
    return;
  }

  if (try_termination_context.request->scale_in_hook_name().empty()) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_GROUP_NAME_REQUIRED);
    SCP_ERROR_CONTEXT(kGcpAutoScalingClientProvider, try_termination_context,
                      execution_result, "Invalid request.");
    try_termination_context.result = execution_result;
    try_termination_context.Finish();
    return;
  }

  auto request = make_shared<GetInstanceByNameRequest>();
  auto instance_name_or = GcpInstanceClientUtils::ToInstanceName(
      try_termination_context.request->instance_resource_name());
  if (!instance_name_or.result().Successful()) {
    try_termination_context.result = instance_name_or.result();
    SCP_ERROR_CONTEXT(
        kGcpAutoScalingClientProvider, try_termination_context,
        try_termination_context.result,
        "Cannot construct the instance name from the input "
        "instance resource name: %s.",
        try_termination_context.request->instance_resource_name().c_str());
    try_termination_context.Finish();
    return;
  }
  request->set_instance_name(move(*instance_name_or));

  AsyncContext<GetInstanceByNameRequest, GetInstanceByNameResponse>
      get_instance_context(
          move(request),
          bind(&GcpAutoScalingClientProvider::OnGetInstanceByNameCallback, this,
               try_termination_context, _1),
          try_termination_context);
  instance_database_client_provider_->GetInstanceByName(get_instance_context);
}

void GcpAutoScalingClientProvider::OnGetInstanceByNameCallback(
    AsyncContext<TryFinishInstanceTerminationRequest,
                 TryFinishInstanceTerminationResponse>& try_termination_context,
    AsyncContext<GetInstanceByNameRequest, GetInstanceByNameResponse>&
        get_instance_context) noexcept {
  if (!get_instance_context.result.Successful()) {
    // Instance not found in the database means it is not scheduled to be
    // terminated.
    if (get_instance_context.result ==
        FailureExecutionResult(
            SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND)) {
      try_termination_context.response =
          make_shared<TryFinishInstanceTerminationResponse>();
      try_termination_context.response->set_termination_scheduled(false);
      try_termination_context.result = SuccessExecutionResult();
      try_termination_context.Finish();
      return;
    }
    try_termination_context.result = get_instance_context.result;
    SCP_ERROR_CONTEXT(
        kGcpAutoScalingClientProvider, try_termination_context,
        try_termination_context.result, "Failed to get instance for %s.",
        try_termination_context.request->instance_resource_name().c_str());
    try_termination_context.Finish();
    return;
  }

  const auto& instance = get_instance_context.response->instance();
  const auto& instance_status = instance.status();
  try_termination_context.response =
      make_shared<TryFinishInstanceTerminationResponse>();
  // Return directly if the instance is not scheduled to be terminated.
  if (instance_status != InstanceStatus::TERMINATING_WAIT) {
    try_termination_context.response->set_termination_scheduled(false);
    try_termination_context.result = SuccessExecutionResult();
    try_termination_context.Finish();
    return;
  }

  RegionInstanceGroupManagersDeleteInstancesRequest delete_instance_request;
  *delete_instance_request.add_instances() = instance.instance_name();

  instance_group_managers_client_
      ->DeleteInstances(current_project_id_, current_region_,
                        try_termination_context.request->scale_in_hook_name(),
                        delete_instance_request)
      .then(bind(&GcpAutoScalingClientProvider::OnDeleteInstanceCallback, this,
                 try_termination_context, _1));
}

void GcpAutoScalingClientProvider::OnDeleteInstanceCallback(
    AsyncContext<TryFinishInstanceTerminationRequest,
                 TryFinishInstanceTerminationResponse>& try_termination_context,
    future<StatusOr<Operation>> delete_result) noexcept {
  // DeleteInstance is a long-run operation. We will not wait and check the
  // result in the operation.
  auto delete_status = delete_result.get();
  auto execution_result =
      common::GcpUtils::GcpErrorConverter(delete_status.status());

  if (!execution_result.Successful()) {
    try_termination_context.result = execution_result;
    SCP_ERROR_CONTEXT(
        kGcpAutoScalingClientProvider, try_termination_context,
        execution_result, "Failed to delete instance %s",
        try_termination_context.request->instance_resource_name().c_str());
    try_termination_context.Finish();
    return;
  }

  try_termination_context.response->set_termination_scheduled(true);
  try_termination_context.result = SuccessExecutionResult();
  try_termination_context.Finish();
}

Options InstanceGroupManagersClientFactory::CreateClientOptions(
    const shared_ptr<AutoScalingClientOptions>& options) noexcept {
  return Options();
}

shared_ptr<RegionInstanceGroupManagersClient>
InstanceGroupManagersClientFactory::CreateClient(
    const shared_ptr<AutoScalingClientOptions>& options) noexcept {
  return make_shared<RegionInstanceGroupManagersClient>(
      MakeRegionInstanceGroupManagersConnectionRest(
          CreateClientOptions(options)));
}

shared_ptr<InstanceDatabaseClientProviderInterface>
InstanceDatabaseClientProviderFactory::CreateClient(
    const shared_ptr<AutoScalingClientOptions>& client_options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  auto instance_database_client_options =
      make_shared<InstanceDatabaseClientOptions>(
          client_options->gcp_spanner_instance_name,
          client_options->gcp_spanner_database_name,
          client_options->instance_table_name);
  return make_shared<GcpInstanceDatabaseClientProvider>(
      instance_database_client_options, instance_client, cpu_async_executor,
      io_async_executor,
      make_shared<GcpDatabaseFactory>(
          dynamic_pointer_cast<DatabaseClientOptions>(
              instance_database_client_options)));
}

#ifndef TEST_CPIO
shared_ptr<AutoScalingClientProviderInterface>
AutoScalingClientProviderFactory::Create(
    const shared_ptr<AutoScalingClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpAutoScalingClientProvider>(
      options, instance_client_provider, cpu_async_executor, io_async_executor);
}
#endif
}  // namespace google::scp::cpio::client_providers
