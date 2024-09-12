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

#include "metric_client_utils.h"

#include <map>
#include <memory>

#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/src/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_METRIC_CLIENT_PROVIDER_INCONSISTENT_NAMESPACE;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NAME_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_VALUE_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET;
using std::shared_ptr;

namespace google::scp::cpio::client_providers {
ExecutionResult MetricClientUtils::ValidateRequest(
    const PutMetricsRequest& request,
    const shared_ptr<MetricClientOptions>& options) {
  if (options->enable_batch_recording) {
    /// If the namespace is set in the request, the namespace should be the same
    /// with the namespace for batch recording if batch recording is enabled.
    if (!request.metric_namespace().empty() &&
        options->namespace_for_batch_recording != request.metric_namespace()) {
      return FailureExecutionResult(
          SC_METRIC_CLIENT_PROVIDER_INCONSISTENT_NAMESPACE);
    }
  } else if (request.metric_namespace().empty()) {
    // The namespace should be set in the request if batch recording is not
    // enabled.
    return FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET);
  }

  if (request.metrics().empty()) {
    return FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET);
  }
  for (auto metric : request.metrics()) {
    if (metric.name().empty()) {
      return FailureExecutionResult(
          SC_METRIC_CLIENT_PROVIDER_METRIC_NAME_NOT_SET);
    }
    if (metric.value().empty()) {
      return FailureExecutionResult(
          SC_METRIC_CLIENT_PROVIDER_METRIC_VALUE_NOT_SET);
    }
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio::client_providers
