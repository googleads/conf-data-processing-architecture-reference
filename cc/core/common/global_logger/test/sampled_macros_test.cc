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
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/logger_interface.h"
#include "core/logger/mock/mock_logger.h"
#include "core/logger/src/log_providers/console_log_provider.h"

using google::scp::core::AsyncContext;
using google::scp::core::LogLevel;
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
using testing::AllOf;
using testing::Contains;
using testing::Each;
using testing::HasSubstr;
using testing::SizeIs;

namespace google::scp::core::test {

class SampledMacrosTest : public testing::Test {
 protected:
  SampledMacrosTest() {
    auto mock_logger = make_unique<MockLogger>();
    logger_ = mock_logger.get();
    unique_ptr<LoggerInterface> logger = move(mock_logger);
    logger->Init();
    logger->Run();
    GlobalLogger::SetGlobalLogger(move(logger));
  }

  ~SampledMacrosTest() { GlobalLogger::GetGlobalLogger()->Stop(); }

  MockLogger* logger_;
};

TEST_F(SampledMacrosTest, ActivityLogsEveryTime) {
  for (int i = 0; i < 5; i++) {
    SCP_INFO_EVERY_N(1, "component", kZeroUuid, "msg s");
  }
  EXPECT_THAT(logger_->GetMessages(), SizeIs(5));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(SampledMacrosTest, ActivityLogsEveryOther) {
  for (int i = 0; i < 5; i++) {
    SCP_INFO_EVERY_N(2, "component", kZeroUuid, "msg s");
  }
  EXPECT_THAT(logger_->GetMessages(), SizeIs(3));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(SampledMacrosTest, ActivityLogsEveryN) {
  for (int i = 0; i < 1000; i++) {
    SCP_INFO_EVERY_N(10, "component", kZeroUuid, "msg s");
  }
  EXPECT_THAT(logger_->GetMessages(), SizeIs(1000 / 10));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(SampledMacrosTest, ActivityNoScopeCollision) {
  for (int i = 0; i < 1000; i++) {
    SCP_INFO_EVERY_N(10, "component", kZeroUuid, "msg s");
    SCP_INFO_EVERY_N(50, "component", kZeroUuid, "msg t");
  }
  EXPECT_THAT(logger_->GetMessages(),
              Contains(HasSubstr("msg s")).Times(1000 / 10));
  EXPECT_THAT(logger_->GetMessages(),
              Contains(HasSubstr("msg t")).Times(1000 / 50));
}

TEST_F(SampledMacrosTest, ContextLogsEveryTime) {
  AsyncContext<int, int> context;
  for (int i = 0; i < 5; i++) {
    SCP_INFO_CONTEXT_EVERY_N(1, "component", context, "msg s");
  }
  EXPECT_THAT(logger_->GetMessages(), SizeIs(5));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(SampledMacrosTest, ContextLogsEveryOther) {
  AsyncContext<int, int> context;
  for (int i = 0; i < 5; i++) {
    SCP_INFO_CONTEXT_EVERY_N(2, "component", context, "msg s");
  }
  EXPECT_THAT(logger_->GetMessages(), SizeIs(3));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(SampledMacrosTest, ContextLogsEveryN) {
  AsyncContext<int, int> context;
  for (int i = 0; i < 1000; i++) {
    SCP_INFO_CONTEXT_EVERY_N(10, "component", context, "msg s");
  }
  EXPECT_THAT(logger_->GetMessages(), SizeIs(1000 / 10));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

TEST_F(SampledMacrosTest, ContextNoScopeCollision) {
  AsyncContext<int, int> context;
  for (int i = 0; i < 1000; i++) {
    SCP_INFO_CONTEXT_EVERY_N(10, "component", context, "msg s");
    SCP_INFO_CONTEXT_EVERY_N(50, "component", context, "msg t");
  }
  EXPECT_THAT(logger_->GetMessages(),
              Contains(HasSubstr("msg s")).Times(1000 / 10));
  EXPECT_THAT(logger_->GetMessages(),
              Contains(HasSubstr("msg t")).Times(1000 / 50));
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

#define LevelTestBodyNoResult(level)                               \
  {                                                                \
    auto activity_id = Uuid::GenerateUuid();                       \
    for (int i = 0; i < 5; i++) {                                  \
      SCP_##level##_EVERY_N(1, "component", activity_id, "msg s"); \
    }                                                              \
    EXPECT_THAT(logger_->GetMessages(), SizeIs(5));                \
    EXPECT_THAT(logger_->GetMessages(),                            \
                AllOf(Each(HasSubstr(GetStringForLevel(#level))),  \
                      Each(HasSubstr(ToString(activity_id)))));    \
    logger_->ClearMessages();                                      \
  }

#define LevelTestBodyWithResult(level)                                    \
  {                                                                       \
    auto activity_id = Uuid::GenerateUuid();                              \
    for (int i = 0; i < 5; i++) {                                         \
      SCP_##level##_EVERY_N(1, "component", activity_id,                  \
                            FailureExecutionResult(SC_UNKNOWN), "msg s"); \
    }                                                                     \
    EXPECT_THAT(logger_->GetMessages(), SizeIs(5));                       \
    EXPECT_THAT(logger_->GetMessages(),                                   \
                AllOf(Each(HasSubstr(GetStringForLevel(#level))),         \
                      Each(HasSubstr(ToString(activity_id)))));           \
    logger_->ClearMessages();                                             \
  }

TEST_F(SampledMacrosTest, WorksForAllLevels) {
  LevelTestBodyNoResult(INFO);
  LevelTestBodyNoResult(DEBUG);
  LevelTestBodyNoResult(WARNING);
  LevelTestBodyWithResult(ERROR);
  LevelTestBodyWithResult(EMERGENCY);
  LevelTestBodyWithResult(ALERT);
  LevelTestBodyWithResult(CRITICAL);
}

void HelperFoo() {
  for (int i = 0; i < 1000; i++) {
    SCP_INFO_EVERY_N(10, "component", kZeroUuid, "msg s");
  }
}

TEST_F(SampledMacrosTest, MultithreadTest) {
  vector<thread> threads;
  for (int i = 0; i < thread::hardware_concurrency(); i++) {
    threads.emplace_back(bind(&HelperFoo));
  }
  for (auto& t : threads) {
    t.join();
  }
  EXPECT_THAT(logger_->GetMessages(), SizeIs((1000 * threads.size()) / 10));
  EXPECT_THAT(logger_->GetMessages(), Each(HasSubstr("msg s")));
}

}  // namespace google::scp::core::test
