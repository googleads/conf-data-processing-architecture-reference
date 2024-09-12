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

#pragma once

namespace google::scp::core::test {

class TestLoggingUtils {
 public:
  /**
   * @brief Enables Logging output to console StdOut
   *
   */
  static void EnableLogOutputToConsole();

  /**
   * @brief Enables Logging output to /var/log/syslog
   *
   */
  static void EnableLogOutputToSyslog();

  /**
   * @brief Enables Logging output to mock logger.
   * The mock logger does not log the output but store log messages into a list.
   * This is only used for unit tests to get test coverage of logs.
   */
  static void EnableLogOutputToMockLogger();
};

};  // namespace google::scp::core::test
