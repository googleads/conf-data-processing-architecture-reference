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

#include "cc/core/interface/errors.h"
#include "public/cpio/interface/error_codes.h"

namespace google::scp::core::errors {
/// Registers component code as 0x0209 for NoSQL database provider.
REGISTER_COMPONENT_CODE(SC_INSTANCE_DATABASE_CLIENT_PROVIDER, 0x0209)

DEFINE_ERROR_CODE(SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND,
                  SC_INSTANCE_DATABASE_CLIENT_PROVIDER, 0x0001,
                  "Database record not found.", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INITIALIZATION_FAILED,
                  SC_INSTANCE_DATABASE_CLIENT_PROVIDER, 0x0002,
                  "Failed to initialize InstanceDatabaseClientProvider.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_INSTANCE_DATABASE_CLIENT_PROVIDER_UNEXPECTED_COLUMN_COUNT,
                  SC_INSTANCE_DATABASE_CLIENT_PROVIDER, 0x0003,
                  "The returned column count is not as expected.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INVALID_INSTANCE_STATUS,
                  SC_INSTANCE_DATABASE_CLIENT_PROVIDER, 0x0004,
                  "Invalid instance status from cloud.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SCP_INSTANCE_DATABASE_CLIENT_PROVIDER_COMMIT_FAILED,
                  SC_INSTANCE_DATABASE_CLIENT_PROVIDER, 0x0005,
                  "Failed to commit transaction.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_INSTANCE_DATABASE_CLIENT_PROVIDER_RECORD_NOT_FOUND,
                         SC_CPIO_ENTITY_NOT_FOUND)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INITIALIZATION_FAILED,
    SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_UNEXPECTED_COLUMN_COUNT,
    SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_INSTANCE_DATABASE_CLIENT_PROVIDER_INVALID_INSTANCE_STATUS,
    SC_CPIO_INTERNAL_ERROR)
MAP_TO_PUBLIC_ERROR_CODE(SCP_INSTANCE_DATABASE_CLIENT_PROVIDER_COMMIT_FAILED,
                         SC_CPIO_INTERNAL_ERROR)
}  // namespace google::scp::core::errors
