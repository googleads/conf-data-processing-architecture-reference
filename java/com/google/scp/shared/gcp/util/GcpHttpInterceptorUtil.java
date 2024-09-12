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

package com.google.scp.shared.gcp.util;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.IdTokenCredentials;
import com.google.auth.oauth2.IdTokenProvider;
import com.google.auth.oauth2.IdTokenProvider.Option;
import com.google.auth.oauth2.ImpersonatedCredentials;
import com.google.common.collect.ImmutableList;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

/** Provide gcp http interceptors */
public final class GcpHttpInterceptorUtil {

  /** Create http interceptor for gcp http clients with url as audience */
  public static org.apache.http.HttpRequestInterceptor createHttpInterceptor(String url) {
    return (request, context) -> {
      String identityToken = getIdTokenFromMetadataServer(url);
      request.addHeader("Authorization", String.format("Bearer %s", identityToken));
    };
  }

  /**
   * Create http interceptor for gcp http clients with url as audience. The generated token is
   * issued for the service account to impersonate, with the requested scopes.
   */
  public static org.apache.http.HttpRequestInterceptor createHttpInterceptor(
      String targetAudienceUrl, String serviceAccountToImpersonate, ImmutableList<String> scopes) {
    return (request, context) -> {
      String identityToken =
          getImpersonatedIdToken(targetAudienceUrl, serviceAccountToImpersonate, scopes);
      request.addHeader("Authorization", String.format("Bearer %s", identityToken));
    };
  }

  /**
   * Create HTTP Interceptor for GCP HTTP clients with url as audience. This is for use when
   * communicating with the PBS server - not with GCP directly.
   *
   * @param url The TargetAudience url to use
   * @return A HttpRequestInterceptor that applies the generated GCP AccessToken to the intercepted
   *     HTTP request.
   */
  public static org.apache.hc.core5.http.HttpRequestInterceptor createPbsHttpInterceptor(
      String url) {
    return (request, entityDetails, context) -> {
      request.addHeader("x-auth-token", getIdTokenFromMetadataServer(url));
    };
  }

  private static IdTokenCredentials getIdTokenCredentials(String url) throws IOException {
    GoogleCredentials googleCredentials = GoogleCredentials.getApplicationDefault();

    return buildIdTokenCredentials((IdTokenProvider) googleCredentials, url, false);
  }

  private static IdTokenCredentials getImpersonatedIdTokenCredentials(
      String targetAudienceUrl, String serviceAccountToImpersonate, ImmutableList<String> scopes)
      throws IOException {
    ImpersonatedCredentials impersonatedCredentials =
        ImpersonatedCredentials.newBuilder()
            .setSourceCredentials(GoogleCredentials.getApplicationDefault())
            .setTargetPrincipal(serviceAccountToImpersonate)
            .setScopes(scopes)
            .build();

    return buildIdTokenCredentials(impersonatedCredentials, targetAudienceUrl, true);
  }

  private static IdTokenCredentials buildIdTokenCredentials(
      IdTokenProvider idTokenProvider, String targetAudienceUrl, boolean includeEmail) {
    var options = new ArrayList<>(Arrays.asList(Option.FORMAT_FULL, Option.LICENSES_TRUE));
    if (includeEmail) {
      options.add(Option.INCLUDE_EMAIL);
    }
    return IdTokenCredentials.newBuilder()
        .setIdTokenProvider(idTokenProvider)
        .setTargetAudience(targetAudienceUrl)
        // Setting the ID token options.
        .setOptions(options)
        .build();
  }

  private static String getIdTokenFromMetadataServer(String url) throws IOException {
    IdTokenCredentials idTokenCredentials = getIdTokenCredentials(url);
    return idTokenCredentials.refreshAccessToken().getTokenValue();
  }

  private static String getImpersonatedIdToken(
      String targetAudienceUrl, String serviceAccountToImpersonate, ImmutableList<String> scopes)
      throws IOException {
    return getImpersonatedIdTokenCredentials(targetAudienceUrl, serviceAccountToImpersonate, scopes)
        .refreshAccessToken()
        .getTokenValue();
  }
}
