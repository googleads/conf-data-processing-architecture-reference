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

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/interface/type_def.h"

using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::cmrt::sdk::private_key_service::v1::PrivateKeyEndpoint;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::PrivateKeyClientFactory;
using google::scp::cpio::PrivateKeyClientInterface;
using google::scp::cpio::PrivateKeyClientOptions;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::stod;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::chrono::milliseconds;

constexpr char kPrivateKeyEndpoint1[] = "https://test.privatekey1.com";
constexpr char kPrivateKeyEndpoint2[] = "https://test.privatekey2.com";
constexpr char kIamRole1[] = "arn:aws:iam::1234:role/test_assume_role_1";
constexpr char kIamRole2[] = "arn:aws:iam::1234:role/test_assume_role_2";
constexpr char kServiceRegion[] = "us-east-1";
constexpr char kKeyId1[] = "key-id";

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  PrivateKeyClientOptions private_key_client_options;
  auto private_key_client =
      PrivateKeyClientFactory::Create(move(private_key_client_options));
  result = private_key_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init private key client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = private_key_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run private key client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }

  std::cout << "Run private key client successfully!" << std::endl;

  auto request = make_shared<ListPrivateKeysRequest>();
  request->add_key_ids(kKeyId1);
  PrivateKeyEndpoint endpoint_1;
  endpoint_1.set_account_identity(kIamRole1);
  endpoint_1.set_key_service_region(kServiceRegion);
  endpoint_1.set_endpoint(kPrivateKeyEndpoint1);
  PrivateKeyEndpoint endpoint_2;
  endpoint_2.set_account_identity(kIamRole2);
  endpoint_2.set_key_service_region(kServiceRegion);
  endpoint_2.set_endpoint(kPrivateKeyEndpoint2);
  *request->add_key_endpoints() = std::move(endpoint_1);
  *request->add_key_endpoints() = std::move(endpoint_2);

  atomic<bool> finished = false;
  auto list_private_keys_context =
      AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>(
          move(request),
          [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>
                  context) {
            if (!context.result.Successful()) {
              std::cout << "ListPrivateKeys failed: "
                        << GetErrorMessage(context.result.status_code)
                        << std::endl;
            } else {
              std::cout << "ListPrivateKeys succeeded." << std::endl;
            }
            finished = true;
          });
  private_key_client->ListPrivateKeys(list_private_keys_context);
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::milliseconds(100000));

  result = private_key_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop private key client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
