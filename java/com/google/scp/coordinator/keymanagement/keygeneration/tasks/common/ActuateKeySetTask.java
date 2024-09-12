/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common;

import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyset.KeySetConfig;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyset.KeySetManager;
import com.google.scp.shared.api.exception.ServiceException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/* This class actuates the state and the keys of the key set against the key sets configurtaion.  */
public final class ActuateKeySetTask {

  private static final Logger logger = LoggerFactory.getLogger(ActuateKeySetTask.class);

  private final KeySetManager keySetManager;
  private final CreateSplitKeyTask createSplitKeyTask;

  @Inject
  public ActuateKeySetTask(KeySetManager keySetManager, CreateSplitKeyTask createSplitKeyTask) {
    this.keySetManager = keySetManager;
    this.createSplitKeyTask = createSplitKeyTask;
  }

  /** Executes the task. */
  public void execute() throws ServiceException {
    ImmutableList<KeySetConfig> configs = keySetManager.getConfigs();
    logger.info("Found {} key sets from the configuration.", configs.size());
    for (KeySetConfig config : configs) {
      logger.info("Key set config: {}.", config.toString());
      createSplitKeyTask.create(
          config.getName(),
          config.getTinkTemplate(),
          config.getCount(),
          config.getValidityInDays(),
          config.getTtlInDays());
    }
  }
}
