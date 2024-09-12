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

#include <gmock/gmock.h>

#include <stdexcept>
#include <string>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"

namespace google::scp::core::test {
/// Matcher to compare protos.
MATCHER_P(EqualsProto, expected, "") {
  std::string explanation;
  protobuf::util::MessageDifferencer differ;
  differ.ReportDifferencesToString(&explanation);
  if (!differ.Compare(expected, arg)) {
    *result_listener << explanation;
    return false;
  }
  return true;
}

MATCHER(EqualsProto, "") {
  const auto& [actual, expected] = arg;
  return testing::ExplainMatchResult(EqualsProto(expected), actual,
                                     result_listener);
}

/**
 * @brief Helper function which accepts a textformatted proto.
 *
 * e.g. message Proto {
 *   int32 i = 1;
 *   string s = 2;
 * }
 *
 * ...
 *
 * auto p = SubstituteAndParseTextToProto<Proto>(R"-(
 *            i: 1
 *            s: "string"
 *          )-");
 *
 * It also accepts a variadic argument list which conforms to absl::Substitute
 * rules.
 *
 * auto p = SubstituteAndParseTextToProto<Proto>(R"-(
 *            i: $0
 *            s: "$1"
 *          )-", int_value, string_value);
 */
template <typename T, typename... Args>
T SubstituteAndParseTextToProto(std::string_view format, Args... args) {
  static_assert(std::is_base_of_v<google::protobuf::Message, T>);
  T proto;
  auto formatted_str = absl::Substitute(format, args...);
  google::protobuf::io::ArrayInputStream is(formatted_str.data(),
                                            formatted_str.length());
  if (!google::protobuf::TextFormat::Parse(&is, &proto)) {
    throw std::runtime_error(absl::StrCat("Unable to parse ", formatted_str,
                                          " into ",
                                          T::descriptor()->full_name()));
  }
  return proto;
}

}  // namespace google::scp::core::test
