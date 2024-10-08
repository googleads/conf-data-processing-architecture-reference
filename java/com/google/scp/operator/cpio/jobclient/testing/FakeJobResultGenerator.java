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

package com.google.scp.operator.cpio.jobclient.testing;

import static com.google.scp.operator.protos.shared.backend.ReturnCodeProto.ReturnCode.SUCCESS;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.scp.operator.cpio.jobclient.model.Job;
import com.google.scp.operator.cpio.jobclient.model.JobResult;
import com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary;
import com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo;
import com.google.scp.shared.proto.ProtoUtil;
import java.time.Instant;

/** Generates {@link JobResult}s for use in tests */
public final class FakeJobResultGenerator {

  public static JobResult fromJob(Job job) {
    return JobResult.builder()
        .setJobKey(job.jobKey())
        .setResultInfo(
            ResultInfo.newBuilder()
                .setReturnCode(SUCCESS.name())
                .setReturnMessage("")
                .setFinishedAt(ProtoUtil.toProtoTimestamp(Instant.parse("2021-01-01T00:00:00Z")))
                .setErrorSummary(
                    ErrorSummary.newBuilder()
                        .setNumReportsWithErrors(0)
                        .addAllErrorCounts(ImmutableList.of())
                        .build())
                .putAllResultMetadata(
                    ImmutableMap.of("number of files processed", "30", "job duration", "4.5 hours"))
                .build())
        .build();
  }
}
