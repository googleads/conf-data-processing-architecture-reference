/*
 * Copyright 2023 Google LLC
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

#include <future>
#include <memory>
#include <string>
#include <utility>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio {

class SyncUtils {
 public:
  /**
   * @brief Accepts a function func that starts some operation on another thread
   * (probably on AsyncExecutor). Executes this function and waits for the async
   * operation to complete, then returns the status and fills response upon
   * success.
   *
   * @tparam RequestT
   * @tparam ResponseT
   * @tparam Func A template parameter to allow for binding to a std::function,
   * c-style function pointer, or a lambda.
   * @param func The function to execute which completes its work
   * asynchronously. This function should be equivalent to
   * std::function<ExecutionResult(AsyncContext<RequestT, ResponseT>&)>. We
   * don't use a std::function in the signature because that limits the user to
   * have a std::function. This function should set context.result,
   * context.response on success and call context.Finish().
   * @param request The request to the asynchronous operation.
   * @param response The response of the asynchronous operation - filled on
   * success.
   * @return core::ExecutionResult Whether the asynchronous operation was
   * successful.
   */
  // The RequestT or ResponseT cannot be void or primitive types.
  template <typename RequestT, typename ResponseT, typename Func>
  static core::ExecutionResult AsyncToSync(Func&& func, RequestT request,
                                           ResponseT& response) noexcept {
    std::promise<std::pair<core::ExecutionResult, std::shared_ptr<ResponseT>>>
        request_promise;
    core::AsyncContext<RequestT, ResponseT> context;
    context.request = std::make_shared<RequestT>(std::move(request));
    context.callback = [&](core::AsyncContext<RequestT, ResponseT>& outcome) {
      request_promise.set_value({outcome.result, outcome.response});
    };

    auto execution_result = func(context);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    auto [result, actual_response] = request_promise.get_future().get();
    RETURN_IF_FAILURE(result);
    response = std::move(*actual_response);
    return core::SuccessExecutionResult();
  }

  // TODO: Rename to ExecuteNetworkCall after migrating ExecutionResult as the
  // returned value in CPIO.
  template <typename RequestT, typename ResponseT, typename Func>
  static core::ExecutionResult AsyncToSync2(Func&& func, RequestT request,
                                            ResponseT& response) noexcept {
    std::promise<std::pair<core::ExecutionResult, std::shared_ptr<ResponseT>>>
        request_promise;
    core::AsyncContext<RequestT, ResponseT> context;
    context.request = std::make_shared<RequestT>(std::move(request));
    context.callback = [&](core::AsyncContext<RequestT, ResponseT>& outcome) {
      request_promise.set_value({outcome.result, outcome.response});
    };

    func(context);

    auto [result, actual_response] = request_promise.get_future().get();
    RETURN_IF_FAILURE(result);
    response = std::move(*actual_response);
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::cpio
