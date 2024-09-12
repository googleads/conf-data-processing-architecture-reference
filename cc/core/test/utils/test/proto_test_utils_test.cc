/*
 * Copyright 2024 Google LLC
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
#include "core/test/utils/proto_test_utils.h"

#include "core/test/utils/test/test.pb.h"

namespace google::scp::core::test {

TEST(ParseTextProtoTest, ParsingWorks) {
  auto tp = SubstituteAndParseTextToProto<TestProto>("");
  EXPECT_THAT(tp, EqualsProto(TestProto::default_instance()));

  tp = SubstituteAndParseTextToProto<TestProto>(
      "i: 1 s: \"string\" b: true tiny_proto { i64: 2 }");
  TestProto expected;
  expected.set_i(1);
  expected.set_s("string");
  expected.set_b(true);
  expected.mutable_tiny_proto()->set_i64(2);
  EXPECT_THAT(tp, EqualsProto(expected));

  // Test that Raw string works.
  tp = SubstituteAndParseTextToProto<TestProto>(R"-(
      i: 1
      s: "string"
      b: true
      tiny_proto {
        i64: 2
      }
      )-");
  EXPECT_THAT(tp, EqualsProto(expected));
}

TEST(ParseTextProtoTest, ParsingWithFormatStringWorks) {
  auto tp = SubstituteAndParseTextToProto<TestProto>(R"-(
      i: $0
      s: "$1"
      b: $2
      tiny_proto {
        i64: $3
      }
      )-",
                                                     1, "string", true, 2);
  TestProto expected;
  expected.set_i(1);
  expected.set_s("string");
  expected.set_b(true);
  expected.mutable_tiny_proto()->set_i64(2);
  EXPECT_THAT(tp, EqualsProto(expected));
}

TEST(ParseTextProtoTest, ParsingFailsOnInvalidField) {
  EXPECT_ANY_THROW(SubstituteAndParseTextToProto<TestProto>(R"-(
      i: 1
      my_new_field: "fourteen"
      )-"));
}

}  // namespace google::scp::core::test
