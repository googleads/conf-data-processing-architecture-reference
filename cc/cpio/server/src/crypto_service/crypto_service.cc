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
#include <execinfo.h>
#include <unistd.h>

#include <csignal>
#include <filesystem>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "core/async_executor/src/async_executor.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/errors.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/crypto_client_provider/src/crypto_client_provider.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/crypto_client/crypto_client_interface.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/proto/crypto_service/v1/configuration_keys.pb.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.grpc.pb.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::ClientConfigurationKeys;
using google::cmrt::sdk::crypto_service::v1::ClientConfigurationKeys_Name;
using google::cmrt::sdk::crypto_service::v1::CryptoService;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeParams;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::Logger;
using google::scp::cpio::CryptoClientInterface;
using google::scp::cpio::CryptoClientOptions;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::ExecuteSyncCall;
using google::scp::cpio::Init;
using google::scp::cpio::kCryptoServiceAddress;
using google::scp::cpio::ReadConfigInt;
using google::scp::cpio::ReadConfigStringList;
using google::scp::cpio::Run;
using google::scp::cpio::RunConfigProvider;
using google::scp::cpio::RunLogger;
using google::scp::cpio::RunServer;
using google::scp::cpio::SignalSegmentationHandler;
using google::scp::cpio::Stop;
using google::scp::cpio::StopLogger;
using google::scp::cpio::TryReadConfigInt;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::CryptoClientProvider;
using std::bind;
using std::cout;
using std::endl;
using std::list;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;

namespace {
constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;

constexpr char kConfigProviderName[] = "config_provider";
constexpr char kCryptoClientName[] = "crypto_client";
}  // namespace

shared_ptr<ConfigProviderInterface> config_provider;
shared_ptr<CryptoClientInterface> crypto_client;

class CryptoServiceImpl : public CryptoService::CallbackService {
 public:
  grpc::ServerUnaryReactor* HpkeEncrypt(
      grpc::CallbackServerContext* server_context,
      const HpkeEncryptRequest* request,
      HpkeEncryptResponse* response) override {
    return ExecuteSyncCall<HpkeEncryptRequest, HpkeEncryptResponse>(
        server_context, *request, *response,
        bind(&CryptoClientInterface::HpkeEncryptSync, crypto_client, _1));
  }

  grpc::ServerUnaryReactor* HpkeDecrypt(
      grpc::CallbackServerContext* server_context,
      const HpkeDecryptRequest* request,
      HpkeDecryptResponse* response) override {
    return ExecuteSyncCall<HpkeDecryptRequest, HpkeDecryptResponse>(
        server_context, *request, *response,
        bind(&CryptoClientInterface::HpkeDecryptSync, crypto_client, _1));
  }

  grpc::ServerUnaryReactor* AeadEncrypt(
      grpc::CallbackServerContext* server_context,
      const AeadEncryptRequest* request,
      AeadEncryptResponse* response) override {
    return ExecuteSyncCall<AeadEncryptRequest, AeadEncryptResponse>(
        server_context, *request, *response,
        bind(&CryptoClientInterface::AeadEncryptSync, crypto_client, _1));
  }

  grpc::ServerUnaryReactor* AeadDecrypt(
      grpc::CallbackServerContext* server_context,
      const AeadDecryptRequest* request,
      AeadDecryptResponse* response) override {
    return ExecuteSyncCall<AeadDecryptRequest, AeadDecryptResponse>(
        server_context, *request, *response,
        bind(&CryptoClientInterface::AeadDecryptSync, crypto_client, _1));
  }
};

static void SignalHandler(int signum) {
  Stop(crypto_client, kCryptoClientName);
  StopLogger();
  Stop(config_provider, kConfigProviderName);
  SignalSegmentationHandler(signum);
  exit(signum);
}

void RegisterRpcHandlers();

void RunClients();

int main(int argc, char* argv[]) {
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGPIPE, SIG_IGN);

  RunConfigProvider(config_provider, kConfigProviderName);

  RunLogger(config_provider);

  RunClients();

  auto num_completion_queues = kDefaultNumCompletionQueues;
  TryReadConfigInt(
      config_provider,
      ClientConfigurationKeys_Name(
          ClientConfigurationKeys::CMRT_CRYPTO_CLIENT_COMPLETION_QUEUE_COUNT),
      num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider,
                   ClientConfigurationKeys_Name(
                       ClientConfigurationKeys::CMRT_CRYPTO_CLIENT_MIN_POLLERS),
                   min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider,
                   ClientConfigurationKeys_Name(
                       ClientConfigurationKeys::CMRT_CRYPTO_CLIENT_MAX_POLLERS),
                   max_pollers);

  CryptoServiceImpl service;
  RunServer<CryptoServiceImpl>(service, kCryptoServiceAddress,
                               num_completion_queues, min_pollers, max_pollers);
  return 0;
}

void RunClients() {
  auto options = make_shared<CryptoClientOptions>();

  crypto_client = make_shared<CryptoClientProvider>(options);
  Init(crypto_client, kCryptoClientName);
  Run(crypto_client, kCryptoClientName);
}
