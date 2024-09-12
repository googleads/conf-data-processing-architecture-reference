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

#ifndef SCP_CORE_INTERFACE_EXECUTION_RESULT_MACROS_H_
#define SCP_CORE_INTERFACE_EXECUTION_RESULT_MACROS_H_

#include "execution_result.h"

namespace google::scp::core {

// Macro to shorten this pattern:
// ExecutionResult result = foo();
// if (!result.Successful()) {
//   return result;
// }
//
// This is useful if the callsite doesn't need to use the ExecutionResult
// anymore than just returning it upon failure.
//
// Example 1:
// ExecutionResult result = foo();
// RETURN_IF_FAILURE(result);
// // If we reach this point, result was Successful.
//
// Example 2:
// RETURN_IF_FAILURE(foo());
// // If we reach this point, foo() was Successful.
#define RETURN_IF_FAILURE(execution_result)                          \
  if (::google::scp::core::ExecutionResult __res = execution_result; \
      !__res.Successful()) {                                         \
    return __res;                                                    \
  }

// Same as above but simply returns void instead of an ExecutionResult.
#define RETURN_VOID_IF_FAILURE(execution_result)                     \
  if (::google::scp::core::ExecutionResult __res = execution_result; \
      !__res.Successful()) {                                         \
    return;                                                          \
  }

// Same as above but logs an error before returning upon failure.
// The other arguments would be the same as those used in SCP_ERROR(...) except
// the ExecutionResult is abstracted away.
//
// Example:
// RETURN_AND_LOG_IF_FAILURE(foo(), kComponentName, parent_activity_id,
//     activity_id, "some message %s", str.c_str()));
// // If we reach this point, foo() was Successful and SCP_ERROR_CONTEXT was not
// // called.
#define RETURN_AND_LOG_IF_FAILURE(execution_result, component_name,           \
                                  activity_id, message, ...)                  \
  __RETURN_IF_FAILURE_LOG(__res, execution_result, SCP_ERROR, component_name, \
                          activity_id, message, ##__VA_ARGS__)

// Same as above but logs an error using the supplied context upon failure.
// The other arguments would be the same as those used in
// SCP_ERROR_CONTEXT(...). RETURN_AND_LOG_IF_FAILURE_CONTEXT(foo(),
// kComponentName, context, "some message %s", str.c_str()));
#define RETURN_AND_LOG_IF_FAILURE_CONTEXT(execution_result, component_name, \
                                          async_context, message, ...)      \
  __RETURN_IF_FAILURE_LOG(__res, execution_result, SCP_ERROR_CONTEXT,       \
                          component_name, async_context, message,           \
                          ##__VA_ARGS__)

// Same as RETURN_AND_LOG_IF_FAILURE but returns void.
#define RETURN_VOID_AND_LOG_IF_FAILURE(execution_result, component_name, \
                                       activity_id, message, ...)        \
  __RETURN_IF_FAILURE_LOG(, execution_result, SCP_ERROR, component_name, \
                          activity_id, message, ##__VA_ARGS__)

// Same as above but accepts an AsyncContext.
#define RETURN_VOID_AND_LOG_IF_FAILURE_CONTEXT(                    \
    execution_result, component_name, async_context, message, ...) \
  __RETURN_IF_FAILURE_LOG(, execution_result, SCP_ERROR_CONTEXT,   \
                          component_name, async_context, message,  \
                          ##__VA_ARGS__)

#define __RETURN_IF_FAILURE_LOG(ret_val, execution_result, error_level,      \
                                component_name, activity_id, message, ...)   \
  if (::google::scp::core::ExecutionResult __res = execution_result;         \
      !__res.Successful()) {                                                 \
    error_level(component_name, activity_id, __res, message, ##__VA_ARGS__); \
    return ret_val;                                                          \
  }

// Macro to shorten this pattern:
// ExecutionResult result = foo();
// if (!result.Successful()) {
//   ERROR(kComponentName, parent_activity_id, activity_id, "some message %s",
//         str.c_str());
// }
//
// This is useful if the callsite doesn't need to use the ExecutionResult
// anymore than just logging it upon failure.
//
// Example:
// LOG_IF_FAILURE(foo(), kComponentName, parent_activity_id,
//     activity_id, "some message %s", str.c_str()));
#define LOG_IF_FAILURE(execution_result, component_name, activity_id, message, \
                       ...)                                                    \
  __LOF_IF_FAILURE(execution_result, SCP_ERROR, component_name, activity_id,   \
                   message, ##__VA_ARGS__)
// Same as above but logs an error using the supplied context upon failure.
// The other arguments would be the same as those used in
// SCP_ERROR_CONTEXT(...).
// LOG_IF_FAILURE_CONTEXT(foo(), kComponentName, context, "some message %s",
//                        str.c_str()));
#define LOG_IF_FAILURE_CONTEXT(execution_result, component_name,        \
                               async_context, message, ...)             \
  __LOF_IF_FAILURE(execution_result, SCP_ERROR_CONTEXT, component_name, \
                   async_context, message, ##__VA_ARGS__)

#define __LOF_IF_FAILURE(execution_result, error_level, component_name,      \
                         activity_id, message, ...)                          \
  if (::google::scp::core::ExecutionResult __res = execution_result;         \
      !__res.Successful()) {                                                 \
    error_level(component_name, activity_id, __res, message, ##__VA_ARGS__); \
  }

}  // namespace google::scp::core

#endif  // SCP_CORE_INTERFACE_EXECUTION_RESULT_MACROS_H_
