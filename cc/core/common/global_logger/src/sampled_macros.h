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
#pragma once

#define __UNIQUE_VAR_NAME_LOG_EVERY_HELPER(x, y) x##y
#define __UNIQUE_VAR_NAME_LOG_EVERY(x, y) \
  __UNIQUE_VAR_NAME_LOG_EVERY_HELPER(x, y)

// Same as SCP_INFO except only prints out every N number of times.
#define SCP_INFO_EVERY_N(count, component_name, activity_id, message, ...)   \
  __SCP_EVERY_N_HELPER(INFO, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), \
                                                                             \
                       count, component_name, activity_id, message,          \
                       ##__VA_ARGS__)

// Same as SCP_DEBUG except only prints out every N number of times.
#define SCP_DEBUG_EVERY_N(count, component_name, activity_id, message, ...)   \
  __SCP_EVERY_N_HELPER(DEBUG, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), \
                                                                              \
                       count, component_name, activity_id, message,           \
                       ##__VA_ARGS__)

// Same as SCP_WARNING except only prints out every N number of times.
#define SCP_WARNING_EVERY_N(count, component_name, activity_id, message, ...) \
  __SCP_EVERY_N_HELPER(WARNING,                                               \
                       __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count, \
                       component_name, activity_id, message, ##__VA_ARGS__)

// Same as SCP_ERROR except only prints out every N number of times.
#define SCP_ERROR_EVERY_N(count, component_name, activity_id, result, message, \
                          ...)                                                 \
  __SCP_EVERY_N_HELPER(ERROR, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__),  \
                                                                               \
                       count, component_name, activity_id, result, message,    \
                       ##__VA_ARGS__)

// Same as SCP_CRITICAL except only prints out every N number of times.
#define SCP_CRITICAL_EVERY_N(count, component_name, activity_id, result, \
                             message, ...)                               \
  __SCP_EVERY_N_HELPER(                                                  \
      CRITICAL, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count,   \
      component_name, activity_id, result, message, ##__VA_ARGS__)

// Same as SCP_ALERT except only prints out every N number of times.
#define SCP_ALERT_EVERY_N(count, component_name, activity_id, result, message, \
                          ...)                                                 \
  __SCP_EVERY_N_HELPER(ALERT, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__),  \
                                                                               \
                       count, component_name, activity_id, result, message,    \
                       ##__VA_ARGS__)

// Same as SCP_EMERGENCY except only prints out every N number of times.
#define SCP_EMERGENCY_EVERY_N(count, component_name, activity_id, result, \
                              message, ...)                               \
  __SCP_EVERY_N_HELPER(                                                   \
      EMERGENCY, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count,   \
      component_name, activity_id, result, message, ##__VA_ARGS__)

// Same as SCP_INFO_CONTEXT except only prints out every N number of times.
#define SCP_INFO_CONTEXT_EVERY_N(count, component_name, async_context,        \
                                 message, ...)                                \
  __SCP_EVERY_N_HELPER(INFO_CONTEXT,                                          \
                       __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count, \
                       component_name, async_context, message, ##__VA_ARGS__)

// Same as SCP_DEBUG_CONTEXT except only prints out every N number of times.
#define SCP_DEBUG_CONTEXT_EVERY_N(count, component_name, async_context,       \
                                  message, ...)                               \
  __SCP_EVERY_N_HELPER(DEBUG_CONTEXT,                                         \
                       __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count, \
                       component_name, async_context, message, ##__VA_ARGS__)

// Same as SCP_WARNING_CONTEXT except only prints out every N number of
// times.
#define SCP_WARNING_CONTEXT_EVERY_N(count, component_name, async_context,     \
                                    message, ...)                             \
  __SCP_EVERY_N_HELPER(WARNING_CONTEXT,                                       \
                       __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count, \
                       component_name, async_context, message, ##__VA_ARGS__)

// Same as SCP_ERROR_CONTEXT except only prints out every N number of times.
#define SCP_ERROR_CONTEXT_EVERY_N(count, component_name, async_context,     \
                                  result, message, ...)                     \
  __SCP_EVERY_N_HELPER(                                                     \
      ERROR_CONTEXT, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count, \
      component_name, async_context, result, message, ##__VA_ARGS__)

// Same as SCP_CRITICAL_CONTEXT except only prints out every N number of
// times.
#define SCP_CRITICAL_CONTEXT_EVERY_N(count, component_name, async_context,     \
                                     result, message, ...)                     \
  __SCP_EVERY_N_HELPER(                                                        \
      CRITICAL_CONTEXT, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count, \
      component_name, async_context, result, message, ##__VA_ARGS__)

// Same as SCP_ALERT_CONTEXT except only prints out every N number of times.
#define SCP_ALERT_CONTEXT_EVERY_N(count, component_name, async_context,     \
                                  result, message, ...)                     \
  __SCP_EVERY_N_HELPER(                                                     \
      ALERT_CONTEXT, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__), count, \
      component_name, async_context, result, message, ##__VA_ARGS__)

// Same as SCP_EMERGENCY_CONTEXT except only prints out every N number of
// times.
#define SCP_EMERGENCY_CONTEXT_EVERY_N(count, component_name, async_context, \
                                      result, message, ...)                 \
  __SCP_EVERY_N_HELPER(                                                     \
      EMERGENCY_CONTEXT, __UNIQUE_VAR_NAME_LOG_EVERY(counter, __LINE__),    \
      count, component_name, async_context, result, message, ##__VA_ARGS__)

// The counter can theoretically overflow, but this is a best effort anyways.
#define __SCP_EVERY_N_HELPER(level, counter, count, component_name,      \
                             activity_id_or_context, message, ...)       \
  static ::std::atomic_uint64_t counter = 0;                             \
  if (uint64_t current_value =                                           \
          counter.fetch_add(1, std::memory_order::memory_order_relaxed); \
      (current_value % count) == 0) {                                    \
    SCP_##level(component_name, activity_id_or_context, message,         \
                ##__VA_ARGS__);                                          \
  }
