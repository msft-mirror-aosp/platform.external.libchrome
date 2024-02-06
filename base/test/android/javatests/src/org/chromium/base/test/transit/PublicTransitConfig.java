// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.widget.Toast;

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

/** Configuration for PublicTransit tests. */
public class PublicTransitConfig {
    private static final String TAG = "Transit";
    private static long sTransitionPause;
    private static Runnable sOnExceptionCallback;
    private static boolean sFreezeOnException;

    /**
     * Set a pause for all transitions for debugging.
     *
     * @param millis how long to pause for (1000 to 4000 ms is typical).
     */
    public static void setTransitionPauseForDebugging(long millis) {
        sTransitionPause = millis;
        ResettersForTesting.register(() -> sTransitionPause = 0);
    }

    /**
     * Set a callback to be run when a {@link TravelException} will be thrown.
     *
     * <p>Useful to print debug information for failures that can't be reproduced with a debugger.
     *
     * @param onExceptionCallback the callback to run on exception.
     */
    public static void setOnExceptionCallback(Runnable onExceptionCallback) {
        sOnExceptionCallback = onExceptionCallback;
        ResettersForTesting.register(() -> sOnExceptionCallback = null);
    }

    /**
     * Set the test to freeze when a {@link TravelException} will be thrown.
     *
     * <p>Useful to watch the test behavior for some time after the Exception. In conjunction with
     * {@link #setOnExceptionCallback(Runnable)}, the callback will be run on an exponential backoff
     * schedule starting at 1 second and doubling from that.
     *
     * <p>Lasts until the test runner times out the test.
     */
    public static void setFreezeOnException() {
        sFreezeOnException = true;
        ResettersForTesting.register(() -> sFreezeOnException = false);
    }

    static void maybePauseAfterTransition(ConditionalState state) {
        long pauseMs = sTransitionPause;
        if (pauseMs > 0) {
            String toastText = buildToastText(state);
            ThreadUtils.runOnUiThread(
                    () -> {
                        Toast.makeText(
                                        InstrumentationRegistry.getInstrumentation()
                                                .getTargetContext(),
                                        toastText,
                                        Toast.LENGTH_SHORT)
                                .show();
                    });
            try {
                Log.e(TAG, "Pause for sightseeing %s for %dms", state, pauseMs);
                Thread.sleep(pauseMs);
            } catch (InterruptedException e) {
                Log.e(TAG, "Interrupted pause", e);
            }
        }
    }

    private static String buildToastText(ConditionalState state) {
        StringBuilder textToDisplay = new StringBuilder();
        String currentTestCase = TrafficControl.getCurrentTestCase();
        if (currentTestCase != null) {
            textToDisplay.append("[");
            textToDisplay.append(currentTestCase);
            textToDisplay.append("]\n");
        }
        textToDisplay.append(state.toString());
        String textToDisplayString = textToDisplay.toString();
        return textToDisplayString;
    }

    static void onTravelException(TravelException travelException) {
        triggerOnExceptionCallback();
        if (sFreezeOnException) {
            int backoffTimer = 1000;
            int totalMsFrozen = 0;

            // Will freeze until the test runner times out.
            while (true) {
                try {
                    Thread.sleep(backoffTimer);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
                totalMsFrozen += backoffTimer;
                backoffTimer = 2 * backoffTimer;
                Log.e(TAG, "Frozen for %sms on TravelException:", totalMsFrozen, travelException);
                triggerOnExceptionCallback();
            }
        }
    }

    private static void triggerOnExceptionCallback() {
        if (sOnExceptionCallback != null) {
            sOnExceptionCallback.run();
        }
    }
}
