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

#ifndef SCP_CORE_INTERFACE_EXECUTION_RESULT_OR_MACROS_H_
#define SCP_CORE_INTERFACE_EXECUTION_RESULT_OR_MACROS_H_

#include "execution_result.h"

namespace google::scp::core {

// Macro similar to RETURN_IF_FAILURE but for ExecutionResultOr.
// Useful for shortening this pattern:
// ExecutionResultOr<Foo> result_or = foo();
// if (!result_or.Successful()) {
//   return result_or.result();
// }
// Foo val = std::move(*result_or);
//
// Example 1:
// // NOTEs:
// // 1. This pattern will not compile if Foo is non-copyable - use Example 2
// //    instead.
// // 2. This pattern results in the value being copied once into an internal
// //    variable.
//
// ExecutionResultOr<Foo> result_or = foo();
// ASSIGN_OR_RETURN(auto val, result_or);
// // If we reach this point, foo() was Successful and val is of type Foo.
//
// Example 2:
// ASSIGN_OR_RETURN(auto val, foo());
// // If we reach this point, foo() was Successful and val is of type Foo.
//
// Example 3:
// Foo val;
// ASSIGN_OR_RETURN(val, foo());
// // If we reach this point, foo() was Successful and val holds the value.
//
// Example 4:
// std::pair<Foo, Bar> pair;
// ASSIGN_OR_RETURN(pair.first, foo());
// // If we reach this point, foo() was Successful and pair.first holds the
// // value.
#define ASSIGN_OR_RETURN(lhs, execution_result_or)                   \
  __ASSIGN_OR_RETURN_HELPER(__res, lhs, __UNIQUE_VAR_NAME(__LINE__), \
                            execution_result_or, )

// Same as above but returns void.
#define ASSIGN_OR_RETURN_VOID(lhs, execution_result_or)         \
  __ASSIGN_OR_RETURN_HELPER(, lhs, __UNIQUE_VAR_NAME(__LINE__), \
                            execution_result_or, )

// Same as above but logs the error before returning it.
// The other arguments would be the same as those used in SCP_ERROR(...) except
// the ExecutionResult is abstracted away.
//
// Example:
// ExecutionResultOr<Foo> result_or = foo();
// ASSIGN_OR_LOG_AND_RETURN(auto val, result_or, kComponentName,
//     parent_activity_id, activity_id, "some message %s", str.c_str());
// // If we reach this point, foo() was Successful and val is of type Foo.
#define ASSIGN_OR_LOG_AND_RETURN(lhs, execution_result_or, component_name, \
                                 activity_id, message, ...)                \
  __ASSIGN_OR_RETURN_HELPER(                                               \
      __res, lhs, __UNIQUE_VAR_NAME(__LINE__), execution_result_or,        \
      SCP_ERROR(component_name, activity_id, __res, message, ##__VA_ARGS__))

// Same as above but logs the error using the supplied context before returning
// it.
// The other arguments would be the same as those used in
// SCP_ERROR_CONTEXT(...) except the ExecutionResult is abstracted away.
//
// Example:
// ExecutionResultOr<Foo> result_or = foo();
// ASSIGN_OR_LOG_AND_RETURN_CONTEXT(auto val, result_or, kComponentName,
//     context, "some message %s", str.c_str());
// // If we reach this point, foo() was Successful and val is of type Foo.
#define ASSIGN_OR_LOG_AND_RETURN_CONTEXT(                                    \
    lhs, execution_result_or, component_name, async_context, message, ...)   \
  __ASSIGN_OR_RETURN_HELPER(__res, lhs, __UNIQUE_VAR_NAME(__LINE__),         \
                            execution_result_or,                             \
                            SCP_ERROR_CONTEXT(component_name, async_context, \
                                              __res, message, ##__VA_ARGS__))

// Same as ASSIGN_OR_LOG_AND_RETURN but returns void.
#define ASSIGN_OR_LOG_AND_RETURN_VOID(                                   \
    lhs, execution_result_or, component_name, activity_id, message, ...) \
  __ASSIGN_OR_RETURN_HELPER(                                             \
      , lhs, __UNIQUE_VAR_NAME(__LINE__), execution_result_or,           \
      SCP_ERROR(component_name, activity_id, __res, message, ##__VA_ARGS__))

// Same as above but accepts an AsyncContext.
#define ASSIGN_OR_LOG_AND_RETURN_VOID_CONTEXT(                               \
    lhs, execution_result_or, component_name, async_context, message, ...)   \
  __ASSIGN_OR_RETURN_HELPER(, lhs, __UNIQUE_VAR_NAME(__LINE__),              \
                            execution_result_or,                             \
                            SCP_ERROR_CONTEXT(component_name, async_context, \
                                              __res, message, ##__VA_ARGS__))

#define __ASSIGN_OR_RETURN_W_LOG(lhs, execution_result_or, failure_statement) \
  __ASSIGN_OR_RETURN_HELPER(__res, lhs, __UNIQUE_VAR_NAME(__LINE__),          \
                            execution_result_or, failure_statement)

#define __UNIQUE_VAR_NAME_HELPER(x, y) x##y
#define __UNIQUE_VAR_NAME(x) __UNIQUE_VAR_NAME_HELPER(__var, x)

#define __ASSIGN_OR_RETURN_HELPER(ret_val, lhs, result_or_temp_var_name,  \
                                  execution_result_or, failure_statement) \
  auto&& result_or_temp_var_name = execution_result_or;                   \
  if (!result_or_temp_var_name.Successful()) {                            \
    auto __res = result_or_temp_var_name.result();                        \
    google::scp::core::internal::IgnoreUnused(__res);                     \
    failure_statement;                                                    \
    return ret_val;                                                       \
  }                                                                       \
  lhs = result_or_temp_var_name.release();

}  // namespace google::scp::core

#endif  // SCP_CORE_INTERFACE_EXECUTION_RESULT_OR_MACROS_H_
