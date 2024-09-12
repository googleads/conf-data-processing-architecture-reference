// Copyright 2023 Google LLC
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

#include "public/cpio/utils/configuration_fetcher/src/configuration_fetcher_utils.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/server/interface/configuration_keys.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/utils/configuration_fetcher/src/error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LogLevel;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::optional;
using std::string;
using testing::UnorderedElementsAre;

namespace google::scp::cpio {
TEST(ConfigurationFetcherUtilsTest, StringToUIntTest) {
  EXPECT_THAT(ConfigurationFetcherUtils::StringToUInt<size_t>("-10"),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToUInt<size_t>("-1"),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
  EXPECT_THAT(
      ConfigurationFetcherUtils::StringToUInt<size_t>("18446744073709551616"),
      ResultIs(
          FailureExecutionResult(SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
  EXPECT_THAT(
      ConfigurationFetcherUtils::StringToUInt<size_t>("18446744073709551615"),
      IsSuccessfulAndHolds(18446744073709551615));
}

TEST(ConfigurationFetcherUtilsTest, StringToBoolTest) {
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("true"),
              IsSuccessfulAndHolds(true));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("True"),
              IsSuccessfulAndHolds(true));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("TRUE"),
              IsSuccessfulAndHolds(true));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("1"),
              IsSuccessfulAndHolds(true));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("false"),
              IsSuccessfulAndHolds(false));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("False"),
              IsSuccessfulAndHolds(false));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("FALSE"),
              IsSuccessfulAndHolds(false));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("0"),
              IsSuccessfulAndHolds(false));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("invalid"),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
  EXPECT_THAT(ConfigurationFetcherUtils::StringToBool("-1"),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
}

TEST(ConfigurationFetcherUtilsTest, StringToEnumTest) {
  EXPECT_THAT(
      ConfigurationFetcherUtils::StringToEnum("Alert", kLogLevelConfigMap),
      IsSuccessfulAndHolds(LogLevel::kAlert));
  EXPECT_THAT(
      ConfigurationFetcherUtils::StringToEnum("Invalid", kLogLevelConfigMap),
      ResultIs(
          FailureExecutionResult(SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
}

TEST(ConfigurationFetcherUtilsTest, StringToEnumSetTest) {
  EXPECT_THAT(*ConfigurationFetcherUtils::StringToEnumSet("Alert,Debug",
                                                          kLogLevelConfigMap),
              UnorderedElementsAre(LogLevel::kAlert, LogLevel::kDebug));
  EXPECT_THAT(
      ConfigurationFetcherUtils::StringToEnumSet("Invalid", kLogLevelConfigMap),
      ResultIs(
          FailureExecutionResult(SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
}
}  // namespace google::scp::cpio
