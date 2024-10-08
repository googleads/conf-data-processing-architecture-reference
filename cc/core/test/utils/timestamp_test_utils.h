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

#include <gtest/gtest.h>

#include <google/protobuf/timestamp.pb.h>

namespace google::scp::core::test {
/**
 * @brief Expect timestamps equal.
 *
 * @param target the target timestamp.
 * @param expected the expected timestamp.
 */
void ExpectTimestampEquals(const google::protobuf::Timestamp& target,
                           const google::protobuf::Timestamp& expected);

/**
 * @brief Construct Timestamp proto.
 *
 * @param seconds seconds for the timestamp.
 * @param nanos nanos for the timestamp.
 * @return google::protobuf::Timestamp created Timestamp.
 */
google::protobuf::Timestamp MakeProtoTimestamp(int64_t seconds, int32_t nanos);
}  // namespace google::scp::core::test
