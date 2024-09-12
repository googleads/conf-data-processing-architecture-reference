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

#ifndef SCP_CPIO_INTERFACE_PARAMETER_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_PARAMETER_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for fetching application metadata stored in
 * cloud.
 *
 * Use ParameterClientFactory::Create to create the ParameterClient. Call
 * ParameterClientInterface::Init and ParameterClientInterface::Run before
 * actually use it, and call ParameterClientInterface::Stop when finish using
 * it.
 */
class ParameterClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Gets parameter value for a given name.
   *
   * @param context the context of the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual void GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          context) noexcept = 0;

  /**
   * @brief Gets parameter value for a given name.
   *
   * @param request the request to get parameter.
   * @return core::ExecutionResultOr<GetParameterResponse> the
   * execution result of the operation.
   */
  virtual core::ExecutionResultOr<
      cmrt::sdk::parameter_service::v1::GetParameterResponse>
  GetParameterSync(cmrt::sdk::parameter_service::v1::GetParameterRequest
                       request) noexcept = 0;
};

/// Factory to create ParameterClient.
class ParameterClientFactory {
 public:
  /**
   * @brief Creates ParameterClient.
   *
   * @param options configurations for ParameterClient.
   * @return std::unique_ptr<ParameterClientInterface> ParameterClient object.
   */
  static std::unique_ptr<ParameterClientInterface> Create(
      ParameterClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_PARAMETER_CLIENT_INTERFACE_H_
