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

#define __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD_HELPER(x, y) x##y
#define __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(x, y) \
  __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD_HELPER(x, y)

// Same as SCP_INFO except only prints out if period has passed since the last
// encounter, where period is a std::duration.
#define SCP_INFO_EVERY_PERIOD(period, component_name, activity_id, message,   \
                              ...)                                            \
  __SCP_EVERY_PERIOD_HELPER(                                                  \
      INFO, __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__), \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,         \
      component_name, activity_id, message, ##__VA_ARGS__)

// Same as SCP_DEBUG except only prints out if period has passed since the last
// encounter, where period is a std::duration.
#define SCP_DEBUG_EVERY_PERIOD(period, component_name, activity_id, message,   \
                               ...)                                            \
  __SCP_EVERY_PERIOD_HELPER(                                                   \
      DEBUG, __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__), \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,          \
      component_name, activity_id, message, ##__VA_ARGS__)

// Same as SCP_WARNING except only prints out if period has passed since the
// last encounter, where period is a std::duration.
#define SCP_WARNING_EVERY_PERIOD(period, component_name, activity_id, message, \
                                 ...)                                          \
  __SCP_EVERY_PERIOD_HELPER(                                                   \
      WARNING,                                                                 \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__),        \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,          \
      component_name, activity_id, message, ##__VA_ARGS__)

// Same as SCP_ERROR except only prints out if period has passed since the last
// encounter, where period is a std::duration.
#define SCP_ERROR_EVERY_PERIOD(period, component_name, activity_id, result,    \
                               message, ...)                                   \
  __SCP_EVERY_PERIOD_HELPER(                                                   \
      ERROR, __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__), \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,          \
      component_name, activity_id, result, message, ##__VA_ARGS__)

// Same as SCP_CRITICAL except only prints out if period has passed since the
// last encounter, where period is a std::duration.
#define SCP_CRITICAL_EVERY_PERIOD(period, component_name, activity_id, result, \
                                  message, ...)                                \
  __SCP_EVERY_PERIOD_HELPER(                                                   \
      CRITICAL,                                                                \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__),        \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,          \
      component_name, activity_id, result, message, ##__VA_ARGS__)

// Same as SCP_ALERT except only prints out if period has passed since the last
// encounter, where period is a std::duration.
#define SCP_ALERT_EVERY_PERIOD(period, component_name, activity_id, result,    \
                               message, ...)                                   \
  __SCP_EVERY_PERIOD_HELPER(                                                   \
      ALERT, __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__), \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,          \
      component_name, activity_id, result, message, ##__VA_ARGS__)

// Same as SCP_EMERGENCY except only prints out if period has passed since the
// last encounter, where period is a std::duration.
#define SCP_EMERGENCY_EVERY_PERIOD(period, component_name, activity_id, \
                                   result, message, ...)                \
  __SCP_EVERY_PERIOD_HELPER(                                            \
      EMERGENCY,                                                        \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__), \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,   \
      component_name, activity_id, result, message, ##__VA_ARGS__)

// Same as SCP_INFO_CONTEXT except only prints out if period has passed since
// the last encounter, where period is a std::duration.
#define SCP_INFO_CONTEXT_EVERY_PERIOD(period, component_name, async_context, \
                                      message, ...)                          \
  __SCP_EVERY_PERIOD_HELPER(                                                 \
      INFO_CONTEXT,                                                          \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__),      \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,        \
      component_name, async_context, message, ##__VA_ARGS__)

// Same as SCP_DEBUG_CONTEXT except only prints out if period has passed since
// the last encounter, where period is a std::duration.
#define SCP_DEBUG_CONTEXT_EVERY_PERIOD(period, component_name, async_context, \
                                       message, ...)                          \
  __SCP_EVERY_PERIOD_HELPER(                                                  \
      DEBUG_CONTEXT,                                                          \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__),       \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,         \
      component_name, async_context, message, ##__VA_ARGS__)

// Same as SCP_WARNING_CONTEXT except only prints out if period has passed since
// the last encounter, where period is a std::duration.
#define SCP_WARNING_CONTEXT_EVERY_PERIOD(period, component_name,        \
                                         async_context, message, ...)   \
  __SCP_EVERY_PERIOD_HELPER(                                            \
      WARNING_CONTEXT,                                                  \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__), \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,   \
      component_name, async_context, message, ##__VA_ARGS__)

// Same as SCP_ERROR_CONTEXT except only prints out if period has passed since
// the last encounter, where period is a std::duration.
#define SCP_ERROR_CONTEXT_EVERY_PERIOD(period, component_name, async_context, \
                                       result, message, ...)                  \
  __SCP_EVERY_PERIOD_HELPER(                                                  \
      ERROR_CONTEXT,                                                          \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__),       \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,         \
      component_name, async_context, result, message, ##__VA_ARGS__)

// Same as SCP_CRITICAL_CONTEXT except only prints out if period has passed
// since the last encounter, where period is a std::duration.
#define SCP_CRITICAL_CONTEXT_EVERY_PERIOD(period, component_name,              \
                                          async_context, result, message, ...) \
  __SCP_EVERY_PERIOD_HELPER(                                                   \
      CRITICAL_CONTEXT,                                                        \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__),        \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,          \
      component_name, async_context, result, message, ##__VA_ARGS__)

// Same as SCP_ALERT_CONTEXT except only prints out if period has passed since
// the last encounter, where period is a std::duration.
#define SCP_ALERT_CONTEXT_EVERY_PERIOD(period, component_name, async_context, \
                                       result, message, ...)                  \
  __SCP_EVERY_PERIOD_HELPER(                                                  \
      ALERT_CONTEXT,                                                          \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__),       \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,         \
      component_name, async_context, result, message, ##__VA_ARGS__)

// Same as SCP_EMERGENCY_CONTEXT except only prints out if period has passed
// since the last encounter, where period is a std::duration.
#define SCP_EMERGENCY_CONTEXT_EVERY_PERIOD(                             \
    period, component_name, async_context, result, message, ...)        \
  __SCP_EVERY_PERIOD_HELPER(                                            \
      EMERGENCY_CONTEXT,                                                \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log_timestamp, __LINE__), \
      __UNIQUE_VAR_NAME_LOG_EVERY_PERIOD(next_log, __LINE__), period,   \
      component_name, async_context, result, message, ##__VA_ARGS__)

#define __SCP_EVERY_PERIOD_HELPER(level, next_log_timestamp, next_log, period, \
                                  component_name, activity_id_or_context,      \
                                  message, ...)                                \
  static ::std::atomic<::std::chrono::steady_clock::time_point>                \
      next_log_timestamp{::std::chrono::steady_clock::time_point()};           \
  auto next_log =                                                              \
      next_log_timestamp.load(std::memory_order::memory_order_relaxed);        \
  if (auto now = ::std::chrono::steady_clock::now(); now >= next_log) {        \
    auto next_log_new = now + period;                                          \
    if (next_log_timestamp.compare_exchange_weak(next_log, next_log_new)) {    \
      SCP_##level(component_name, activity_id_or_context, message,             \
                  ##__VA_ARGS__);                                              \
    }                                                                          \
  }
