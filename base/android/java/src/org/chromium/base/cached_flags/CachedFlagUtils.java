// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import java.util.List;

/**
 * Utility methods for {@link CachedFlag}.
 *
 * <p>TODO(crbug.com/40266922): Rename this to CachedFlagUtils.
 */
public class CachedFlagUtils {
    /** Caches flags that must take effect on startup but are set via native code. */
    public static void cacheNativeFlags(List<CachedFlag> featuresToCache) {
        for (CachedFlag feature : featuresToCache) {
            feature.cacheFeature();
        }
    }

    /** Caches flags that must take effect on startup but are set via native code. */
    public static void cacheFieldTrialParameters(List<CachedFieldTrialParameter> parameters) {
        for (CachedFieldTrialParameter parameter : parameters) {
            parameter.cacheToDisk();
        }
    }

    /**
     * Do not call this from tests.
     *
     * Features.JUnitProcessor and Features.InstrumentationProcessor already reset this state.
     *
     * Exceptions are tests that test the flags infrastructure.
     */
    public static void resetFlagsForTesting() {
        ValuesReturned.clearForTesting();
        ValuesOverridden.removeOverrides();
    }
}
