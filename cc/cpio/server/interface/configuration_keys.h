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

#include <map>
#include <string>

#include "core/interface/logger_interface.h"
#include "public/cpio/interface/type_def.h"

// Common configurations for all SDK services.
namespace google::scp::cpio {
static const std::map<std::string, LogOption> kLogOptionConfigMap = {
    {"NoLog", LogOption::kNoLog},
    {"ConsoleLog", LogOption::kConsoleLog},
    {"SysLog", LogOption::kSysLog}};
static const std::map<std::string, core::LogLevel> kLogLevelConfigMap = {
    {"Alert", core::LogLevel::kAlert},
    {"Critical", core::LogLevel::kCritical},
    {"Debug", core::LogLevel::kDebug},
    {"Emergency", core::LogLevel::kEmergency},
    {"Error", core::LogLevel::kError},
    {"Info", core::LogLevel::kInfo},
    {"Warning", core::LogLevel::kWarning}};
}  // namespace google::scp::cpio
