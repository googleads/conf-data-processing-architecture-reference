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

#include <google/protobuf/util/time_util.h>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/job_client/job_client_interface.h"
#include "public/cpio/interface/job_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"
#include "public/cpio/utils/configuration_fetcher/src/configuration_fetcher.h"
#include "public/cpio/utils/job_lifecycle_helper/proto/v1/job_lifecycle_helper.pb.h"
#include "public/cpio/utils/job_lifecycle_helper/src/error_codes.h"
#include "public/cpio/utils/job_lifecycle_helper/src/job_lifecycle_helper.h"
#include "public/cpio/utils/metric_instance/src/metric_instance_factory.h"

using google::cmrt::sdk::job_lifecycle_helper::v1::
    JobLifecycleHelperMetricOptions;
using google::cmrt::sdk::job_lifecycle_helper::v1::JobLifecycleHelperOptions;
using google::cmrt::sdk::job_lifecycle_helper::v1::MarkJobCompletedRequest;
using google::cmrt::sdk::job_lifecycle_helper::v1::PrepareNextJobRequest;
using google::protobuf::Duration;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_JOB_LIFECYCLE_HELPER_CURRENT_INSTANCE_IS_TERMINATING;
using google::scp::core::errors::SC_JOB_LIFECYCLE_HELPER_INVALID_JOB_STATUS;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::AutoScalingClientFactory;
using google::scp::cpio::AutoScalingClientInterface;
using google::scp::cpio::AutoScalingClientOptions;
using google::scp::cpio::ConfigurationFetcher;
using google::scp::cpio::ConfigurationFetcherInterface;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::GetConfigurationRequest;
using google::scp::cpio::JobClientFactory;
using google::scp::cpio::JobClientInterface;
using google::scp::cpio::JobClientOptions;
using google::scp::cpio::JobLifecycleHelper;
using google::scp::cpio::JobLifecycleHelperInterface;
using google::scp::cpio::LogOption;
using google::scp::cpio::MetricInstanceFactoryInterface;
using google::scp::cpio::NoopMetricInstanceFactory;
using std::atomic;
using std::cerr;
using std::cout;
using std::endl;
using std::make_shared;
using std::make_unique;
using std::map;
using std::nullopt;
using std::shared_ptr;
using std::stod;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::chrono::milliseconds;

constexpr int8_t kDefaultJobRetryLimit = 3;
constexpr int16_t kJobProcessingTimeoutInSeconds = 600;
constexpr int16_t kDefaultExtendingVisibilityTimeoutInSeconds = 500;
constexpr int8_t kDefaultExtendingVisibilityTimeoutSleepTimeInSeconds = 5;
constexpr bool kDefaultEnableMetricsRecording = true;
constexpr char kDefaultMetricNamespace[] = "test-namespace";

unique_ptr<ConfigurationFetcherInterface> CreateConfigurationFetcher();
unique_ptr<JobClientInterface> CreateJobClient(
    unique_ptr<ConfigurationFetcherInterface>& configuration_fetcher);
unique_ptr<AutoScalingClientInterface> CreateAutoScalingClient(
    unique_ptr<ConfigurationFetcherInterface>& configuration_fetcher);
unique_ptr<MetricInstanceFactoryInterface> CreateMetricInstanceFactory();
unique_ptr<JobLifecycleHelperInterface> CreateJobLifecycleHelper(
    unique_ptr<JobClientInterface>& job_client,
    unique_ptr<AutoScalingClientInterface>& auto_scaling_client,
    unique_ptr<MetricInstanceFactoryInterface>& metric_instance_factory,
    unique_ptr<ConfigurationFetcherInterface>& configuration_fetcher);

int main(int argc, char* argv[]) {
  cout << "Start Worker..." << endl;

  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    cout << "Failed to initialize CPIO: " << GetErrorMessage(result.status_code)
         << endl;
  }

  auto configuration_fetcher = CreateConfigurationFetcher();
  auto auto_scaling_client = CreateAutoScalingClient(configuration_fetcher);
  auto job_client = CreateJobClient(configuration_fetcher);
  auto metric_instance_factory = CreateMetricInstanceFactory();
  auto job_lifecycle_helper =
      CreateJobLifecycleHelper(job_client, auto_scaling_client,
                               metric_instance_factory, configuration_fetcher);

  while (true) {
    PrepareNextJobRequest prepare_next_job_request;
    auto prepare_next_job_response =
        job_lifecycle_helper->PrepareNextJobSync(prepare_next_job_request);
    if (!prepare_next_job_response.Successful()) {
      cout << "Failed to Prepare next job: "
           << GetErrorMessage(prepare_next_job_response.result().status_code)
           << endl;
      if (prepare_next_job_response.result().status_code ==
          SC_JOB_LIFECYCLE_HELPER_CURRENT_INSTANCE_IS_TERMINATING) {
        cout << "Stop worker due to current instance is terminating." << endl;
        break;
      }
      sleep(10);
      continue;
    }

    auto job = prepare_next_job_response->job();
    // Customer can now process the job.
    // If job processing failed, customer can use ReleaseJobForRetry in
    // JobLifecycleHelper to release the job to other workers.

    // Mark job completed after processing the job.
    MarkJobCompletedRequest mark_job_completed_request;
    mark_job_completed_request.set_job_id(job.job_id());
    mark_job_completed_request.set_job_status(job.job_status());
    auto mark_job_completed_response =
        job_lifecycle_helper->MarkJobCompletedSync(mark_job_completed_request);
    if (!mark_job_completed_response.Successful()) {
      cout << "Failed to Mark job completed: "
           << GetErrorMessage(mark_job_completed_response.result().status_code)
           << endl;
      sleep(10);
      continue;
    }
  }

  cout << "Stop Worker..." << endl;
  result = job_lifecycle_helper->Stop();
  if (!result.Successful()) {
    cerr << "Failed to Stop JobLifecycelHelper: "
         << GetErrorMessage(result.status_code) << endl;
    return EXIT_FAILURE;
  }
  result = job_client->Stop();
  if (!result.Successful()) {
    cerr << "Failed to Stop JobClient: " << GetErrorMessage(result.status_code)
         << endl;
    return EXIT_FAILURE;
  }
  result = auto_scaling_client->Stop();
  if (!result.Successful()) {
    cerr << "Failed to Stop AutoScalingClient: "
         << GetErrorMessage(result.status_code) << endl;
    return EXIT_FAILURE;
  }
  result = configuration_fetcher->Stop();
  if (!result.Successful()) {
    cerr << "Failed to Stop ConfigurationFetcher: "
         << GetErrorMessage(result.status_code) << endl;
    return EXIT_FAILURE;
  }
  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    cerr << "Failed to Shut down CPIO: " << GetErrorMessage(result.status_code)
         << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

unique_ptr<ConfigurationFetcherInterface> CreateConfigurationFetcher() {
  auto configuration_fetcher =
      make_unique<ConfigurationFetcher>(nullopt, nullopt);
  auto result = configuration_fetcher->Init();
  if (!result.Successful()) {
    cerr << "Failed to Init ConfigurationFetcher: "
         << GetErrorMessage(result.status_code) << endl;
    exit(EXIT_FAILURE);
  }
  result = configuration_fetcher->Run();
  if (!result.Successful()) {
    cerr << "Failed to Run ConfigurationFetcher: "
         << GetErrorMessage(result.status_code) << endl;
    exit(EXIT_FAILURE);
  }
  return configuration_fetcher;
}

unique_ptr<AutoScalingClientInterface> CreateAutoScalingClient(
    unique_ptr<ConfigurationFetcherInterface>& configuration_fetcher) {
  AutoScalingClientOptions options;
  GetConfigurationRequest request;
  auto response =
      configuration_fetcher.get()->GetAutoScalingClientInstanceTableNameSync(
          request);
  if (!response.Successful()) {
    cerr << "Failed to Get AutoScalingClientInstanceTableName: "
         << GetErrorMessage(response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  options.instance_table_name = response.value();
  response =
      configuration_fetcher.get()->GetAutoScalingClientSpannerInstanceNameSync(
          request);
  if (!response.Successful()) {
    cerr << "Failed to Get AutoScalingClientSpannerInstanceName: "
         << GetErrorMessage(response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  options.gcp_spanner_instance_name = response.value();
  response =
      configuration_fetcher.get()->GetAutoScalingClientSpannerDatabaseNameSync(
          request);
  if (!response.Successful()) {
    cerr << "Failed to Get AutoScalingClientSpannerDatabaseName: "
         << GetErrorMessage(response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  options.gcp_spanner_database_name = response.value();

  auto auto_scaling_client = AutoScalingClientFactory::Create(options);
  auto result = auto_scaling_client->Init();
  if (!result.Successful()) {
    cerr << "Failed to Init AutoScalingClient: "
         << GetErrorMessage(result.status_code) << endl;
    exit(EXIT_FAILURE);
  }
  result = auto_scaling_client->Run();
  if (!result.Successful()) {
    cerr << "Failed to Run AutoScalingClient: "
         << GetErrorMessage(result.status_code) << endl;
    exit(EXIT_FAILURE);
  }
  return auto_scaling_client;
}

unique_ptr<JobClientInterface> CreateJobClient(
    unique_ptr<ConfigurationFetcherInterface>& configuration_fetcher) {
  JobClientOptions options;
  GetConfigurationRequest request;
  auto response =
      configuration_fetcher.get()->GetJobClientJobQueueNameSync(request);
  if (!response.Successful()) {
    cerr << "Failed to Get JobClientJobQueueName: "
         << GetErrorMessage(response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  options.job_queue_name = response.value();
  response = configuration_fetcher.get()->GetJobClientJobTableNameSync(request);
  if (!response.Successful()) {
    cerr << "Failed to Get JobClientJobTableName: "
         << GetErrorMessage(response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  options.job_table_name = response.value();
  response =
      configuration_fetcher.get()->GetGcpJobClientSpannerInstanceNameSync(
          request);
  if (!response.Successful()) {
    cerr << "Failed to Get GcpJobClientSpannerInstanceName: "
         << GetErrorMessage(response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  options.gcp_spanner_instance_name = response.value();
  response =
      configuration_fetcher.get()->GetGcpJobClientSpannerDatabaseNameSync(
          request);
  if (!response.Successful()) {
    cerr << "Failed to Get GetGcpJobClientSpannerDatabaseName: "
         << GetErrorMessage(response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  options.gcp_spanner_database_name = response.value();

  auto job_client = JobClientFactory::Create(options);
  auto result = job_client->Init();
  if (!result.Successful()) {
    cerr << "Failed to Init JobClient:" << GetErrorMessage(result.status_code)
         << endl;
    exit(EXIT_FAILURE);
  }
  result = job_client->Run();
  if (!result.Successful()) {
    cerr << "Failed to Run JobClient:" << GetErrorMessage(result.status_code)
         << endl;
    exit(EXIT_FAILURE);
  }
  return job_client;
}

unique_ptr<MetricInstanceFactoryInterface> CreateMetricInstanceFactory() {
  return make_unique<NoopMetricInstanceFactory>();
}

unique_ptr<JobLifecycleHelperInterface> CreateJobLifecycleHelper(
    unique_ptr<JobClientInterface>& job_client,
    unique_ptr<AutoScalingClientInterface>& auto_scaling_client,
    unique_ptr<MetricInstanceFactoryInterface>& metric_instance_factory,
    unique_ptr<ConfigurationFetcherInterface>& configuration_fetcher) {
  JobLifecycleHelperOptions options;
  GetConfigurationRequest request;
  auto response =
      configuration_fetcher.get()->GetJobLifecycleHelperRetryLimitSync(request);
  if (!response.Successful()) {
    options.set_retry_limit(kDefaultJobRetryLimit);
  } else {
    options.set_retry_limit(response.value());
  }
  response =
      configuration_fetcher.get()
          ->GetJobLifecycleHelperVisibilityTimeoutExtendTimeSync(request);
  if (!response.Successful()) {
    *options.mutable_visibility_timeout_extend_time_seconds() =
        TimeUtil::SecondsToDuration(
            kDefaultExtendingVisibilityTimeoutInSeconds);
  } else {
    *options.mutable_visibility_timeout_extend_time_seconds() =
        TimeUtil::SecondsToDuration(response.value());
  }
  response = configuration_fetcher.get()
                 ->GetJobLifecycleHelperJobProcessingTimeoutSync(request);
  if (!response.Successful()) {
    *options.mutable_job_processing_timeout_seconds() =
        TimeUtil::SecondsToDuration(kJobProcessingTimeoutInSeconds);
  } else {
    *options.mutable_job_processing_timeout_seconds() =
        TimeUtil::SecondsToDuration(response.value());
  }
  response =
      configuration_fetcher.get()
          ->GetJobLifecycleHelperJobExtendingWorkerSleepTimeSync(request);
  if (!response.Successful()) {
    *options.mutable_job_extending_worker_sleep_time_seconds() =
        TimeUtil::SecondsToDuration(
            kDefaultExtendingVisibilityTimeoutSleepTimeInSeconds);
  } else {
    *options.mutable_job_extending_worker_sleep_time_seconds() =
        TimeUtil::SecondsToDuration(response.value());
  }
  auto get_string_response =
      configuration_fetcher.get()->GetCurrentInstanceResourceNameSync(request);
  if (!get_string_response.Successful()) {
    cerr << "Failed to Get CurrentInstanceResourceName: "
         << GetErrorMessage(get_string_response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  *options.mutable_current_instance_resource_name() =
      get_string_response.value();
  get_string_response =
      configuration_fetcher.get()->GetAutoScalingClientScaleInHookNameSync(
          request);
  if (!get_string_response.Successful()) {
    cerr << "Failed to Get AutoScalingClientScaleInHookName: "
         << GetErrorMessage(get_string_response.result().status_code) << endl;
    exit(EXIT_FAILURE);
  }
  *options.mutable_scale_in_hook_name() = get_string_response.value();
  JobLifecycleHelperMetricOptions metric_options;
  auto get_bool_response =
      configuration_fetcher.get()
          ->GetJobLifecycleHelperEnableMetricRecordingSync(request);
  if (!get_bool_response.Successful()) {
    metric_options.set_enable_metrics_recording(kDefaultEnableMetricsRecording);
  } else {
    metric_options.set_enable_metrics_recording(get_bool_response.value());
  }
  get_string_response =
      configuration_fetcher.get()->GetJobLifecycleHelperMetricNamespaceSync(
          request);
  if (!get_string_response.Successful()) {
    *metric_options.mutable_metric_namespace() = kDefaultMetricNamespace;
  } else {
    *metric_options.mutable_metric_namespace() = get_string_response.value();
  }
  *options.mutable_metric_options() = metric_options;
  auto job_lifecycle_helper = make_unique<JobLifecycleHelper>(
      job_client.get(), auto_scaling_client.get(),
      metric_instance_factory.get(), options);
  auto result = job_lifecycle_helper->Init();
  if (!result.Successful()) {
    cerr << "Failed to Init JobLifecycleHelper: "
         << GetErrorMessage(result.status_code) << endl;
    exit(EXIT_FAILURE);
  }
  result = job_lifecycle_helper->Run();
  if (!result.Successful()) {
    cerr << "Failed to Run JobLifecycleHelper: "
         << GetErrorMessage(result.status_code) << endl;
    exit(EXIT_FAILURE);
  }

  return job_lifecycle_helper;
}
