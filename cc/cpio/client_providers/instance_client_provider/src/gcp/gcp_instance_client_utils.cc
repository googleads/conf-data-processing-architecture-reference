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

#include "gcp_instance_client_utils.h"

#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using absl::StrCat;
using absl::StrFormat;
using absl::StrSplit;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::errors::SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE;
using std::make_shared;
using std::move;
using std::regex;
using std::regex_match;
using std::shared_ptr;
using std::string;
using std::strlen;
using std::to_string;
using std::vector;

namespace {
constexpr char kGcpInstanceClientUtils[] = "GcpInstanceClientUtils";

// Valid GCP instance resource name format:
// `//compute.googleapis.com/projects/{PROJECT_ID}/zones/{ZONE_ID}/instances/{INSTANCE_ID}`
constexpr char kInstanceResourceNameRegex[] =
    R"(//compute.googleapis.com/projects\/([a-z0-9][a-z0-9-]{5,29})\/zones\/([a-z][a-z0-9-]{5,29})\/instances\/(\d+))";
constexpr char kInstanceResourceNamePrefix[] = R"(//compute.googleapis.com/)";
constexpr int8_t kInstanceResourceNamePrefixSize = 25;
constexpr char kInstanceNamePrefix[] = "https://www.googleapis.com/compute/v1/";

// GCP listing all tags attached to a resource has two kinds of urls.
// For non-location tied resource, like project, it is
// https://cloudresourcemanager.googleapis.com/v3/tagBindings;
// For location tied resource, like COMPUTE ENGINE instance, it is
// https://LOCATION-cloudresourcemanager.googleapis.com/v3/tagBindings
// For more information, see:
// https://cloud.google.com/resource-manager/docs/tags/tags-creating-and-managing#listing_tags
constexpr char kResourceManagerUriFormat[] =
    "https://%scloudresourcemanager.googleapis.com/v3/tagBindings";
constexpr char kLocationsTag[] = "locations";
constexpr char kZonesTag[] = "zones";
constexpr char kRegionsTag[] = "regions";
}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResultOr<string> GcpInstanceClientUtils::GetCurrentProjectId(
    const shared_ptr<InstanceClientProviderInterface>&
        instance_client) noexcept {
  string instance_resource_name;
  RETURN_AND_LOG_IF_FAILURE(instance_client->GetCurrentInstanceResourceNameSync(
                                instance_resource_name),
                            kGcpInstanceClientUtils, kZeroUuid,
                            "Failed getting instance resource name.");

  auto project_id_or =
      ParseProjectIdFromInstanceResourceName(instance_resource_name);
  RETURN_AND_LOG_IF_FAILURE(
      project_id_or.result(), kGcpInstanceClientUtils, kZeroUuid,
      "Failed to parse instance resource name %s to get project ID",
      instance_resource_name.c_str());

  return move(*project_id_or);
}

ExecutionResultOr<string>
GcpInstanceClientUtils::ParseProjectIdFromInstanceResourceName(
    const string& resource_name) noexcept {
  GcpInstanceResourceNameDetails details;
  RETURN_AND_LOG_IF_FAILURE(
      GetInstanceResourceNameDetails(resource_name, details),
      kGcpInstanceClientUtils, kZeroUuid,
      "Failed to get instance resource name details for %s",
      resource_name.c_str());
  return move(details.project_id);
}

ExecutionResultOr<string>
GcpInstanceClientUtils::ParseZoneIdFromInstanceResourceName(
    const string& resource_name) noexcept {
  GcpInstanceResourceNameDetails details;
  RETURN_AND_LOG_IF_FAILURE(
      GetInstanceResourceNameDetails(resource_name, details),
      kGcpInstanceClientUtils, kZeroUuid,
      "Failed to get instance resource name details for %s",
      resource_name.c_str());
  return move(details.zone_id);
}

ExecutionResultOr<string>
GcpInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
    const string& resource_name) noexcept {
  GcpInstanceResourceNameDetails details;
  RETURN_AND_LOG_IF_FAILURE(
      GetInstanceResourceNameDetails(resource_name, details),
      kGcpInstanceClientUtils, kZeroUuid,
      "Failed to get instance resource name details for %s",
      resource_name.c_str());
  return move(details.instance_id);
}

ExecutionResult GcpInstanceClientUtils::ValidateInstanceResourceNameFormat(
    const string& resource_name) noexcept {
  std::regex re(kInstanceResourceNameRegex);
  if (!std::regex_match(resource_name, re)) {
    auto result = FailureExecutionResult(
        SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME);
    RETURN_AND_LOG_IF_FAILURE(
        result, kGcpInstanceClientUtils, kZeroUuid,
        "Resource name %s doesn't match the expected regex.",
        resource_name.c_str());
  }

  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceClientUtils::GetInstanceResourceNameDetails(
    const string& resource_name,
    GcpInstanceResourceNameDetails& detail) noexcept {
  RETURN_AND_LOG_IF_FAILURE(ValidateInstanceResourceNameFormat(resource_name),
                            kGcpInstanceClientUtils, kZeroUuid,
                            "Resource name %s is invalid.",
                            resource_name.c_str());

  string resource_id =
      resource_name.substr(strlen(kInstanceResourceNamePrefix));
  // Splits `projects/project_abc1/zones/us-west1/instances/12345678987654321`
  // to { projects,project_abc1,zones,us-west1,instances,12345678987654321 }
  vector<string> splits = StrSplit(resource_id, "/");
  detail.project_id = splits[1];
  detail.zone_id = splits[3];
  detail.instance_id = splits[5];

  return SuccessExecutionResult();
}

string GcpInstanceClientUtils::CreateRMListTagsUrl(
    const string& resource_name) noexcept {
  vector<string> splits = StrSplit(resource_name, "/");
  auto i = 0;
  while (i < splits.size()) {
    const auto& part = splits.at(i);
    if (part == kZonesTag || part == kLocationsTag || part == kRegionsTag) {
      const auto& location = splits.at(i + 1);

      return StrFormat(kResourceManagerUriFormat, absl::StrCat(location, "-"));
    }
    i++;
  }
  return StrFormat(kResourceManagerUriFormat, "");
}

ExecutionResultOr<string> GcpInstanceClientUtils::ToInstanceName(
    const string& instance_resource_name) noexcept {
  RETURN_IF_FAILURE(ValidateInstanceResourceNameFormat(instance_resource_name));
  // Validated the instance_resource_name is of the format
  // //compute.googleapis.com/projects/{PROJECT_ID}/zones/{ZONE_ID}/instances/{INSTANCE_ID}
  string name = string(kInstanceNamePrefix);
  absl::StrAppend(
      &name, instance_resource_name.substr(kInstanceResourceNamePrefixSize));
  return name;
}

ExecutionResultOr<string> GcpInstanceClientUtils::ExtractRegion(
    const string& zone) noexcept {
  vector<string> splits = absl::StrSplit(zone, "-");
  if (splits.size() != 3) {
    return FailureExecutionResult(SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE);
  }
  absl::StrAppend(&splits[0], "-", splits[1]);
  return splits[0];
}
}  // namespace google::scp::cpio::client_providers
