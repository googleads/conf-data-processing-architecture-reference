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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/interface/async_context.h"
#include "core/logger/mock/mock_logger.h"
#include "core/logger/src/log_providers/console_log_provider.h"

using google::scp::core::AsyncContext;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::logger::mock::MockLogger;
using std::bind;
using std::make_unique;
using std::move;
using std::string;
using std::thread;
using std::to_string;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::this_thread::sleep_for;
using testing::AllOf;
using testing::Contains;
using testing::Each;
using testing::ExplainMatchResult;
using testing::Ge;
using testing::HasSubstr;
using testing::Le;
using testing::SizeIs;

constexpr size_t kAcceptedDeviationCount = 12;

namespace google::scp::core::test {

class PeriodicMacrosTest : public testing::Test {
 protected:
  PeriodicMacrosTest() {
    auto mock_logger = make_unique<MockLogger>();
    logger_ = mock_logger.get();
    unique_ptr<LoggerInterface> logger = move(mock_logger);
    logger->Init();
    logger->Run();
    GlobalLogger::SetGlobalLogger(move(logger));
  }

  ~PeriodicMacrosTest() { GlobalLogger::GetGlobalLogger()->Stop(); }

  MockLogger* logger_;
};

MATCHER_P2(SizeIsBetween, lower_bound_inc, upper_bound_inc, "") {
  return ExplainMatchResult(
      AllOf(SizeIs(Ge(lower_bound_inc)), SizeIs(Le(upper_bound_inc))), arg,
      result_listener);
}

TEST_F(PeriodicMacrosTest, ActivityLogsOnce) {
  for (int i = 0; i < 2; i++) {
    SCP_INFO_EVERY_PERIOD(milliseconds(100), "component", kZeroUuid, "msg s");
    sleep_for(milliseconds(11));
  }

  EXPECT_THAT(logger_->GetMessages(), SizeIs(1));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(PeriodicMacrosTest, ActivityLogsPeriodically) {
  auto start = steady_clock::now();
  for (int i = 0; i < 500; i++) {
    SCP_INFO_EVERY_PERIOD(milliseconds(10), "component", kZeroUuid, "msg s");
    sleep_for(milliseconds(1));
  }
  auto end = steady_clock::now();
  auto duration_ms = duration_cast<milliseconds>(end - start).count();

  auto expected_num_messages = duration_ms / 10;
  int32_t lower_bound = expected_num_messages - kAcceptedDeviationCount;
  int32_t upper_bound = expected_num_messages + kAcceptedDeviationCount;

  EXPECT_THAT(logger_->GetMessages(), SizeIsBetween(lower_bound, upper_bound));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

MATCHER_P2(IsBetween, lower_bound_inc, upper_bound_inc, "") {
  return ExplainMatchResult(AllOf(Ge(lower_bound_inc), Le(upper_bound_inc)),
                            static_cast<int32_t>(arg), result_listener);
}

TEST_F(PeriodicMacrosTest, ActivityNoScopeCollision) {
  auto start = steady_clock::now();
  for (int i = 0; i < 500; i++) {
    SCP_INFO_EVERY_PERIOD(milliseconds(10), "component", kZeroUuid, "msg s");
    SCP_INFO_EVERY_PERIOD(milliseconds(50), "component", kZeroUuid, "msg t");
    sleep_for(milliseconds(1));
  }
  auto end = steady_clock::now();
  auto duration_ms = duration_cast<milliseconds>(end - start).count();

  auto expected_num_messages = duration_ms / 10;
  int32_t lower_bound = expected_num_messages - kAcceptedDeviationCount;
  int32_t upper_bound = expected_num_messages + kAcceptedDeviationCount;

  EXPECT_THAT(
      logger_->GetMessages(),
      Contains(HasSubstr("msg s")).Times(IsBetween(lower_bound, upper_bound)));

  expected_num_messages = duration_ms / 50;
  lower_bound = expected_num_messages - kAcceptedDeviationCount;
  upper_bound = expected_num_messages + kAcceptedDeviationCount;
  EXPECT_THAT(
      logger_->GetMessages(),
      Contains(HasSubstr("msg t")).Times(IsBetween(lower_bound, upper_bound)));
}

TEST_F(PeriodicMacrosTest, ContextLogsOnce) {
  AsyncContext<int, int> context;
  for (int i = 0; i < 2; i++) {
    SCP_INFO_CONTEXT_EVERY_PERIOD(milliseconds(100), "component", context,
                                  "msg s");
    sleep_for(milliseconds(11));
  }

  EXPECT_THAT(logger_->GetMessages(), SizeIs(1));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(PeriodicMacrosTest, ContextLogsPeriodically) {
  AsyncContext<int, int> context;
  auto start = steady_clock::now();
  for (int i = 0; i < 500; i++) {
    SCP_INFO_CONTEXT_EVERY_PERIOD(milliseconds(10), "component", context,
                                  "msg s");
    sleep_for(milliseconds(1));
  }
  auto end = steady_clock::now();
  auto duration_ms = duration_cast<milliseconds>(end - start).count();

  auto expected_num_messages = duration_ms / 10;
  int32_t lower_bound = expected_num_messages - kAcceptedDeviationCount;
  int32_t upper_bound = expected_num_messages + kAcceptedDeviationCount;

  EXPECT_THAT(logger_->GetMessages(), SizeIsBetween(lower_bound, upper_bound));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(PeriodicMacrosTest, ContextNoScopeCollision) {
  AsyncContext<int, int> context;
  auto start = steady_clock::now();
  for (int i = 0; i < 500; i++) {
    SCP_INFO_CONTEXT_EVERY_PERIOD(milliseconds(10), "component", context,
                                  "msg s");
    SCP_INFO_CONTEXT_EVERY_PERIOD(milliseconds(50), "component", context,
                                  "msg t");
    sleep_for(milliseconds(1));
  }
  auto end = steady_clock::now();
  auto duration_ms = duration_cast<milliseconds>(end - start).count();

  auto expected_num_messages = duration_ms / 10;
  int32_t lower_bound = expected_num_messages - kAcceptedDeviationCount;
  int32_t upper_bound = expected_num_messages + kAcceptedDeviationCount;

  EXPECT_THAT(
      logger_->GetMessages(),
      Contains(HasSubstr("msg s")).Times(IsBetween(lower_bound, upper_bound)));

  expected_num_messages = duration_ms / 50;
  lower_bound = expected_num_messages - kAcceptedDeviationCount;
  upper_bound = expected_num_messages + kAcceptedDeviationCount;
  EXPECT_THAT(
      logger_->GetMessages(),
      Contains(HasSubstr("msg t")).Times(IsBetween(lower_bound, upper_bound)));
}

string GetStringForLevel(string level) {
  if (level == "INFO") {
    return to_string(static_cast<int>(LogLevel::kInfo));
  } else if (level == "DEBUG") {
    return to_string(static_cast<int>(LogLevel::kDebug));
  } else if (level == "WARNING") {
    return to_string(static_cast<int>(LogLevel::kWarning));
  } else if (level == "ERROR") {
    return to_string(static_cast<int>(LogLevel::kError));
  } else if (level == "EMERGENCY") {
    return to_string(static_cast<int>(LogLevel::kEmergency));
  } else if (level == "ALERT") {
    return to_string(static_cast<int>(LogLevel::kAlert));
  } else if (level == "CRITICAL") {
    return to_string(static_cast<int>(LogLevel::kCritical));
  } else {
    ADD_FAILURE();
    return "";
  }
}

#define LevelTestBodyNoResult(level)                                        \
  {                                                                         \
    auto activity_id = Uuid::GenerateUuid();                                \
    for (int i = 0; i < 5; i++) {                                           \
      SCP_##level##_EVERY_PERIOD(milliseconds(0), "component", activity_id, \
                                 "msg s");                                  \
    }                                                                       \
    EXPECT_THAT(logger_->GetMessages(), SizeIs(5));                         \
    EXPECT_THAT(logger_->GetMessages(),                                     \
                AllOf(Each(HasSubstr(GetStringForLevel(#level))),           \
                      Each(HasSubstr(ToString(activity_id)))));             \
    logger_->ClearMessages();                                               \
  }

#define LevelTestBodyWithResult(level)                                         \
  {                                                                            \
    auto activity_id = Uuid::GenerateUuid();                                   \
    for (int i = 0; i < 5; i++) {                                              \
      SCP_##level##_EVERY_PERIOD(milliseconds(0), "component", activity_id,    \
                                 FailureExecutionResult(SC_UNKNOWN), "msg s"); \
    }                                                                          \
    EXPECT_THAT(logger_->GetMessages(), SizeIs(5));                            \
    EXPECT_THAT(logger_->GetMessages(),                                        \
                AllOf(Each(HasSubstr(GetStringForLevel(#level))),              \
                      Each(HasSubstr(ToString(activity_id)))));                \
    logger_->ClearMessages();                                                  \
  }

TEST_F(PeriodicMacrosTest, WorksForAllLevels) {
  LevelTestBodyNoResult(INFO);
  LevelTestBodyNoResult(DEBUG);
  LevelTestBodyNoResult(WARNING);
  LevelTestBodyWithResult(ERROR);
  LevelTestBodyWithResult(EMERGENCY);
  LevelTestBodyWithResult(ALERT);
  LevelTestBodyWithResult(CRITICAL);
}

void HelperFoo() {
  for (int i = 0; i < 500; i++) {
    SCP_INFO_EVERY_PERIOD(milliseconds(10), "component", kZeroUuid, "msg s");
    sleep_for(milliseconds(1));
  }
}

TEST_F(PeriodicMacrosTest, MultithreadTest) {
  AsyncContext<int, int> context;
  auto start = steady_clock::now();
  vector<thread> threads;
  for (int i = 0; i < thread::hardware_concurrency(); i++) {
    threads.emplace_back(bind(&HelperFoo));
  }
  for (auto& t : threads) {
    t.join();
  }
  auto end = steady_clock::now();
  auto duration_ms = duration_cast<milliseconds>(end - start).count();

  auto expected_num_messages = duration_ms / 10;
  int32_t lower_bound = expected_num_messages - kAcceptedDeviationCount;
  int32_t upper_bound = expected_num_messages + kAcceptedDeviationCount;

  EXPECT_THAT(logger_->GetMessages(), SizeIsBetween(lower_bound, upper_bound));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

}  // namespace google::scp::core::test
