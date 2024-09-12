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

#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/proto/common.pb.h"
#include "core/interface/async_context.h"
#include "core/logger/mock/mock_logger.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::common::GlobalLogger;
using google::scp::core::logger::mock::MockLogger;
using std::function;
using std::make_unique;
using std::move;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::FieldsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;
using testing::Pointee;
using testing::UnorderedPointwise;

namespace google::scp::core::test {

TEST(MacroTest, RETURN_IF_FAILURETest) {
  {
    auto helper = [](ExecutionResult result,
                     bool& succeeded) -> ExecutionResult {
      RETURN_IF_FAILURE(result);
      succeeded = true;
      return SuccessExecutionResult();
    };

    bool succeeded = false;
    // Basic returns error.
    EXPECT_THAT(helper(ExecutionResult(ExecutionStatus::Failure, 1), succeeded),
                ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
    EXPECT_FALSE(succeeded);
    // Basic returns success.
    succeeded = false;
    EXPECT_THAT(helper(SuccessExecutionResult(), succeeded), IsSuccessful());
    EXPECT_TRUE(succeeded);
  }

  {
    auto helper = [](function<ExecutionResult()> fun,
                     bool& succeeded) -> ExecutionResult {
      RETURN_IF_FAILURE(fun());
      succeeded = true;
      return SuccessExecutionResult();
    };
    // Returns error.
    bool succeeded = false;
    EXPECT_THAT(
        helper([]() { return ExecutionResult(ExecutionStatus::Failure, 1); },
               succeeded),
        ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
    EXPECT_FALSE(succeeded);

    // Returns success.
    succeeded = false;
    EXPECT_THAT(helper([]() { return SuccessExecutionResult(); }, succeeded),
                IsSuccessful());
    EXPECT_TRUE(succeeded);

    // Calls exactly once on failure.
    succeeded = false;
    int call_count = 0;
    EXPECT_THAT(helper(
                    [&call_count]() {
                      call_count++;
                      return ExecutionResult(ExecutionStatus::Failure, 1);
                    },
                    succeeded),
                ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
    EXPECT_FALSE(succeeded);
    EXPECT_EQ(call_count, 1);

    // Calls exactly once on success.
    succeeded = false;
    call_count = 0;
    EXPECT_THAT(helper(
                    [&call_count]() {
                      call_count++;
                      return SuccessExecutionResult();
                    },
                    succeeded),
                IsSuccessful());
    EXPECT_TRUE(succeeded);
    EXPECT_EQ(call_count, 1);
  }
}

TEST(MacroTest, RETURN_VOID_IF_FAILURETest) {
  auto helper = [](ExecutionResult result, bool& succeeded) {
    RETURN_VOID_IF_FAILURE(result);
    succeeded = true;
  };

  bool succeeded = false;
  // Basic returns error.
  helper(ExecutionResult(ExecutionStatus::Failure, 1), succeeded);
  EXPECT_FALSE(succeeded);
  // Basic returns success.
  succeeded = false;
  helper(SuccessExecutionResult(), succeeded);
  EXPECT_TRUE(succeeded);
}

class MacroLogTest : public testing::Test {
 protected:
  MacroLogTest() {
    auto mock_logger = make_unique<MockLogger>();
    logger_ = mock_logger.get();
    unique_ptr<LoggerInterface> logger = move(mock_logger);
    logger->Init();
    logger->Run();
    GlobalLogger::SetGlobalLogger(std::move(logger));
  }

  ~MacroLogTest() { GlobalLogger::GetGlobalLogger()->Stop(); }

  MockLogger* logger_;
};

TEST_F(MacroLogTest, RETURN_IF_FAILURELogTest) {
  auto helper1 = [](ExecutionResult result) -> ExecutionResult {
    string some_str = "s";
    AsyncContext<int, int> ctx;
    RETURN_AND_LOG_IF_FAILURE_CONTEXT(result, "component", ctx, "msg %s",
                                      some_str.c_str());
    return SuccessExecutionResult();
  };
  // Doesn't log with context.
  EXPECT_THAT(helper1(SuccessExecutionResult()), IsSuccessful());
  EXPECT_THAT(logger_->GetMessages(), IsEmpty());
  // Logs with context.
  EXPECT_THAT(helper1(FailureExecutionResult(SC_UNKNOWN)),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg s")));

  auto helper2 = [](ExecutionResult result) -> ExecutionResult {
    string some_str = "s";
    RETURN_AND_LOG_IF_FAILURE(result, "component", common::kZeroUuid, "msg %s",
                              some_str.c_str());
    return SuccessExecutionResult();
  };
  // Doesn't log without context.
  EXPECT_THAT(helper2(SuccessExecutionResult()), IsSuccessful());
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg s")));
  // Logs without context.
  EXPECT_THAT(helper2(FailureExecutionResult(SC_UNKNOWN)),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  EXPECT_THAT(logger_->GetMessages(),
              ElementsAre(HasSubstr("msg s"), HasSubstr("msg s")));
}

TEST_F(MacroLogTest, RETURN_VOID_IF_FAILURELogTest) {
  auto helper1 = [](ExecutionResult result, ExecutionResult& output) {
    string some_str = "s";
    AsyncContext<int, int> ctx;
    RETURN_VOID_AND_LOG_IF_FAILURE_CONTEXT(result, "component", ctx, "msg %s",
                                           some_str.c_str());
    output = result;
  };
  ExecutionResult output = FailureExecutionResult(SC_UNKNOWN);
  // Doesn't log with context.
  helper1(SuccessExecutionResult(), output);
  EXPECT_SUCCESS(output);
  EXPECT_THAT(logger_->GetMessages(), IsEmpty());
  // Logs with context.
  helper1(FailureExecutionResult(SC_UNKNOWN), output);
  EXPECT_SUCCESS(output);
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg s")));

  auto helper2 = [](ExecutionResult result, ExecutionResult& output) {
    string some_str = "s";
    RETURN_VOID_AND_LOG_IF_FAILURE(result, "component", common::kZeroUuid,
                                   "msg %s", some_str.c_str());
    output = result;
  };
  output = FailureExecutionResult(SC_UNKNOWN);
  // Doesn't log without context.
  helper2(SuccessExecutionResult(), output);
  EXPECT_SUCCESS(output);
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg s")));
  // Logs without context.
  helper2(FailureExecutionResult(SC_UNKNOWN), output);
  EXPECT_SUCCESS(output);
  EXPECT_THAT(logger_->GetMessages(),
              ElementsAre(HasSubstr("msg s"), HasSubstr("msg s")));
}

TEST(MacroTest, ASSIGN_OR_RETURNBasicTest) {
  auto helper = [](ExecutionResultOr<int> result_or,
                   int& val) -> ExecutionResult {
    ASSIGN_OR_RETURN(val, result_or);
    // Call ASSIGN_OR_RETURN again in the same scope to test that temp variables
    // have unique names.
    ASSIGN_OR_RETURN(val, ExecutionResultOr<int>(val));
    val++;
    return SuccessExecutionResult();
  };

  int val;
  ExecutionResultOr<int> result_or(5);
  EXPECT_THAT(helper(result_or, val), IsSuccessful());
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  EXPECT_THAT(helper(result_or, val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_EQ(val, 0);
}

TEST(MacroTest, ASSIGN_OR_RETURN_VOIDBasicTest) {
  auto helper = [](ExecutionResultOr<int> result_or, int& val) {
    ASSIGN_OR_RETURN_VOID(val, result_or);
    // Call ASSIGN_OR_RETURN again in the same scope to test that temp variables
    // have unique names.
    ASSIGN_OR_RETURN_VOID(val, ExecutionResultOr<int>(val));
    val++;
  };

  int val;
  ExecutionResultOr<int> result_or(5);
  helper(result_or, val);
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  helper(result_or, val);
  EXPECT_EQ(val, 0);
}

TEST_F(MacroLogTest, ASSIGN_OR_RETURNLogTest) {
  auto helper1 = [](ExecutionResultOr<int> result_or,
                    int& val) -> ExecutionResult {
    AsyncContext<int, int> ctx;
    ASSIGN_OR_LOG_AND_RETURN_CONTEXT(val, result_or, "component", ctx, "msg %d",
                                     val);
    val++;
    return SuccessExecutionResult();
  };

  int val;
  ExecutionResultOr<int> result_or(5);
  EXPECT_THAT(helper1(result_or, val), IsSuccessful());
  EXPECT_THAT(logger_->GetMessages(), IsEmpty());
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  EXPECT_THAT(helper1(result_or, val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg 0")));
  EXPECT_EQ(val, 0);

  auto helper2 = [](ExecutionResultOr<int> result_or,
                    int& val) -> ExecutionResult {
    ASSIGN_OR_LOG_AND_RETURN(val, result_or, "component", common::kZeroUuid,
                             "msg %d", val);
    val++;
    return SuccessExecutionResult();
  };

  result_or = 5;
  EXPECT_THAT(helper2(result_or, val), IsSuccessful());
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg 0")));
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  EXPECT_THAT(helper2(result_or, val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_THAT(logger_->GetMessages(),
              ElementsAre(HasSubstr("msg 0"), HasSubstr("msg 0")));
  EXPECT_EQ(val, 0);
}

TEST_F(MacroLogTest, ASSIGN_OR_RETURN_VOIDLogTest) {
  auto helper1 = [](ExecutionResultOr<int> result_or, int& val) {
    AsyncContext<int, int> ctx;
    ASSIGN_OR_LOG_AND_RETURN_VOID_CONTEXT(val, result_or, "component", ctx,
                                          "msg %d", val);
    val++;
  };

  int val;
  ExecutionResultOr<int> result_or(5);
  helper1(result_or, val);
  EXPECT_THAT(logger_->GetMessages(), IsEmpty());
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  helper1(result_or, val);
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg 0")));
  EXPECT_EQ(val, 0);

  auto helper2 = [](ExecutionResultOr<int> result_or, int& val) {
    ASSIGN_OR_LOG_AND_RETURN_VOID(val, result_or, "component",
                                  common::kZeroUuid, "msg %d", val);
    val++;
  };

  result_or = 5;
  helper2(result_or, val);
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg 0")));
  EXPECT_EQ(val, 6);

  val = 0;
  result_or = ExecutionResult(ExecutionStatus::Failure, 1);
  helper2(result_or, val);
  EXPECT_THAT(logger_->GetMessages(),
              ElementsAre(HasSubstr("msg 0"), HasSubstr("msg 0")));
  EXPECT_EQ(val, 0);
}

TEST(MacroTest, ASSIGN_OR_RETURNFunctionTest) {
  auto helper = [](function<ExecutionResultOr<int>()> fun,
                   int& val) -> ExecutionResult {
    ASSIGN_OR_RETURN(val, fun());
    val++;
    return SuccessExecutionResult();
  };
  int val;
  EXPECT_THAT(helper([]() -> ExecutionResultOr<int> { return 5; }, val),
              IsSuccessful());
  EXPECT_EQ(val, 6);

  val = 0;
  EXPECT_THAT(helper(
                  []() -> ExecutionResultOr<int> {
                    return ExecutionResult(ExecutionStatus::Failure, 1);
                  },
                  val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_EQ(val, 0);

  // Success executes once.
  val = 0;
  int call_count = 0;
  EXPECT_THAT(helper(
                  [&call_count]() -> ExecutionResultOr<int> {
                    call_count++;
                    return 5;
                  },
                  val),
              IsSuccessful());
  EXPECT_EQ(val, 6);
  EXPECT_EQ(call_count, 1);

  // Failure executes once.
  val = 0;
  call_count = 0;
  EXPECT_THAT(helper(
                  [&call_count]() -> ExecutionResultOr<int> {
                    call_count++;
                    return ExecutionResult(ExecutionStatus::Failure, 1);
                  },
                  val),
              ResultIs(ExecutionResult(ExecutionStatus::Failure, 1)));
  EXPECT_EQ(val, 0);
  EXPECT_EQ(call_count, 1);
}

TEST(MacroTest, ASSIGN_OR_RETURNDeclareWorksInline) {
  auto helper = [](ExecutionResultOr<int> result_or) -> ExecutionResultOr<int> {
    ASSIGN_OR_RETURN(auto ret, result_or);
    return ret;
  };
  EXPECT_THAT(helper(5), IsSuccessfulAndHolds(Eq(5)));
}

TEST(MacroTest, ASSIGN_OR_RETURNWorksWithInnerMembers) {
  auto helper = [](ExecutionResultOr<int> result_or) -> ExecutionResultOr<int> {
    pair<int, string> pair;
    ASSIGN_OR_RETURN(pair.first, result_or);
    return pair.first;
  };
  EXPECT_THAT(helper(5), IsSuccessfulAndHolds(Eq(5)));
}

TEST_F(MacroLogTest, LOG_IF_FAILURELogTest) {
  auto helper1 = [](ExecutionResult result, ExecutionResult& output) {
    string some_str = "s";
    AsyncContext<int, int> ctx;
    LOG_IF_FAILURE_CONTEXT(result, "component", ctx, "msg %s",
                           some_str.c_str());
    output = result;
  };
  ExecutionResult output = FailureExecutionResult(SC_UNKNOWN);
  // Doesn't log with context.
  helper1(SuccessExecutionResult(), output);
  EXPECT_SUCCESS(output);
  EXPECT_THAT(logger_->GetMessages(), IsEmpty());
  // Logs with context.
  helper1(FailureExecutionResult(SC_UNKNOWN), output);
  EXPECT_THAT(output, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg s")));

  auto helper2 = [](ExecutionResult result, ExecutionResult& output) {
    string some_str = "s";
    LOG_IF_FAILURE(result, "component", common::kZeroUuid, "msg %s",
                   some_str.c_str());
    output = result;
  };
  output = FailureExecutionResult(SC_UNKNOWN);
  // Doesn't log without context.
  helper2(SuccessExecutionResult(), output);
  EXPECT_SUCCESS(output);
  EXPECT_THAT(logger_->GetMessages(), ElementsAre(HasSubstr("msg s")));
  // Logs without context.
  helper2(FailureExecutionResult(SC_UNKNOWN), output);
  EXPECT_THAT(output, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  EXPECT_THAT(logger_->GetMessages(),
              ElementsAre(HasSubstr("msg s"), HasSubstr("msg s")));
}

class NoCopyNoDefault {
 public:
  NoCopyNoDefault() = delete;
  NoCopyNoDefault(const NoCopyNoDefault&) = delete;
  NoCopyNoDefault& operator=(const NoCopyNoDefault&) = delete;

  NoCopyNoDefault(NoCopyNoDefault&&) = default;
  NoCopyNoDefault& operator=(NoCopyNoDefault&&) = default;

  explicit NoCopyNoDefault(unique_ptr<int> x) : x_(move(x)) {}

  unique_ptr<int> x_;
};

TEST(MacroTest, ASSIGN_OR_RETURNWorksWithTemporaryNonCopyableTypes) {
  auto helper1 = [](ExecutionResultOr<NoCopyNoDefault> result_or)
      -> ExecutionResultOr<NoCopyNoDefault> {
    auto foo = [&result_or]() { return move(result_or); };
    ASSIGN_OR_RETURN(auto ret, foo());
    return ret;
  };
  EXPECT_THAT(helper1(NoCopyNoDefault(make_unique<int>(5))),
              IsSuccessfulAndHolds(FieldsAre(Pointee(Eq(5)))));

  auto helper2 = [](ExecutionResultOr<NoCopyNoDefault> result_or)
      -> ExecutionResultOr<NoCopyNoDefault> {
    ASSIGN_OR_RETURN(auto ret, move(result_or));
    return ret;
  };
  EXPECT_THAT(helper2(NoCopyNoDefault(make_unique<int>(5))),
              IsSuccessfulAndHolds(FieldsAre(Pointee(Eq(5)))));
}

}  // namespace google::scp::core::test
