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

#include "public/cpio/utils/sync_utils/src/sync_utils.h"

#include <gtest/gtest.h>

#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::function;

namespace google::scp::cpio {

class Request {};

class Response {};

TEST(SyncUtilsTest, AsyncToSyncWorksWithCStyleFunc) {
  ExecutionResult (*fun)(AsyncContext<Request, Response>&) = [](auto& context) {
    context.result = SuccessExecutionResult();
    context.Finish();
    return SuccessExecutionResult();
  };
  Request req;
  Response resp;
  EXPECT_SUCCESS(SyncUtils::AsyncToSync(fun, req, resp));
}

TEST(SyncUtilsTest, AsyncToSyncWorksWithFunction) {
  function<ExecutionResult(AsyncContext<Request, Response>&)> fun =
      [](auto& context) {
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      };
  Request req;
  Response resp;
  EXPECT_SUCCESS(SyncUtils::AsyncToSync(fun, req, resp));
}

TEST(SyncUtilsTest, AsyncToSyncWorksWithFunctionRef) {
  function<ExecutionResult(AsyncContext<Request, Response>&)> fun =
      [](auto& context) {
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      };
  const auto& ref = fun;
  Request req;
  Response resp;
  EXPECT_SUCCESS(SyncUtils::AsyncToSync(ref, req, resp));
}

TEST(SyncUtilsTest, AsyncToSyncWorksWithLambda) {
  auto fun = [](auto& context) {
    context.result = SuccessExecutionResult();
    context.Finish();
    return SuccessExecutionResult();
  };
  Request req;
  Response resp;
  EXPECT_SUCCESS(SyncUtils::AsyncToSync(fun, req, resp));
}

TEST(SyncUtilsTest, AsyncToSyncWorksWithLambdaInPlace) {
  Request req;
  Response resp;
  EXPECT_SUCCESS(SyncUtils::AsyncToSync(
      [](auto& context) {
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      },
      req, resp));
}

}  // namespace google::scp::cpio
