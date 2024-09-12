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

package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp.testing;

import com.google.inject.AbstractModule;
import com.google.inject.Key;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.Annotations.CacheControlMaximum;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbConfig;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbModule;
import com.google.scp.coordinator.keymanagement.testutils.gcp.SpannerKeyDbTestUtil;

/** Module for configuring settings inside a local key hosting emulator. */
public final class KeyHostingHttpFunctionModule extends AbstractModule {
  @Override
  protected void configure() {
    bind(Integer.class).annotatedWith(KeyLimit.class).toInstance(5);
    bind(Long.class).annotatedWith(CacheControlMaximum.class).toInstance(604800L);
    bind(Long.class)
        .annotatedWith(Annotations.CacheControlMaximum.class)
        .to(Key.get(Long.class, CacheControlMaximum.class));
    bind(SpannerKeyDbConfig.class).toInstance(SpannerKeyDbTestUtil.getSpannerKeyDbConfig());
    install(new SpannerKeyDbModule());
  }
}
