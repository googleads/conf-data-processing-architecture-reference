/*
 * Copyright 2023 Google LLC
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

#ifndef SCP_CPIO_INTERFACE_NOSQL_DATABASE_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_NOSQL_DATABASE_CLIENT_TYPE_DEF_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "type_def.h"

namespace google::scp::cpio {
// Convenience wrapper around a <string, optional<string>> pair.
// The two members are 1. The name of the Partition Key for the table
// 2. The name of the Sort Key for the table. Nullopt for no Sort Key.
struct PartitionAndSortKey
    : public std::pair<std::string, std::optional<std::string>> {
  std::string GetPartitionKey() const { return this->first; }

  void SetPartitionKey(const std::string& part_key) { this->first = part_key; }

  std::optional<std::string> GetSortKey() const { return this->second; }

  void SetSortKey(const std::string& sort_key) { this->second = sort_key; }

  void SetNoSortKey() { this->second = std::nullopt; }
};

// Options to give to a NoSQLDatabaseClientProvider.
struct NoSQLDatabaseClientOptions : public DatabaseClientOptions {
  virtual ~NoSQLDatabaseClientOptions() = default;

  NoSQLDatabaseClientOptions() = default;

  NoSQLDatabaseClientOptions(
      std::string input_instance_name, std::string input_database_name,
      std::unique_ptr<std::unordered_map<std::string, PartitionAndSortKey>>
          input_table_name_to_keys)
      : DatabaseClientOptions(std::move(input_instance_name),
                              std::move(input_database_name)),
        gcp_table_name_to_keys(std::move(input_table_name_to_keys)) {}

  NoSQLDatabaseClientOptions(const NoSQLDatabaseClientOptions& options)
      : DatabaseClientOptions(options.gcp_spanner_instance_name,
                              options.gcp_spanner_database_name),
        gcp_table_name_to_keys(
            std::make_unique<
                std::unordered_map<std::string, PartitionAndSortKey>>(
                *options.gcp_table_name_to_keys)) {}

  // Optional argument mapping names of tables to the corresponding partition
  // and (optional) sort keys for that table. This is used to validate calls to
  // Get* and Upsert*. Nullptr to not validate these fields.
  std::unique_ptr<std::unordered_map<std::string, PartitionAndSortKey>>
      gcp_table_name_to_keys = std::make_unique<
          std::unordered_map<std::string, PartitionAndSortKey>>();
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_NOSQL_DATABASE_CLIENT_TYPE_DEF_H_
