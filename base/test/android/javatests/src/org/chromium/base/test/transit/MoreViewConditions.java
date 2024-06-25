// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.Supplier;

/**
 * More specific {@link Condition}s related to Android {@link View}s.
 *
 * <p>{@link ViewConditions} contains the main Conditions generated by {@link ViewElement}s, which
 * supply the {@link View} matched. {@link MoreViewConditions} contains more specific, rarely used
 * Conditions which use the {@link View} matched from the Supplier.
 */
public class MoreViewConditions {

    /** Condition that the supplied View has exactly the expected number of children. */
    public static class ViewHasChildrenCountCondition extends UiThreadCondition {

        private final Supplier<View> mViewSupplier;
        private final int mExpectedCount;

        public ViewHasChildrenCountCondition(Supplier<View> viewSupplier, int expectedCount) {
            mViewSupplier = dependOnSupplier(viewSupplier, "View");
            mExpectedCount = expectedCount;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            ViewGroup group = (ViewGroup) mViewSupplier.get();
            int actualCount = group.getChildCount();
            return whether(
                    actualCount == mExpectedCount,
                    "%s has %d children, expected %d",
                    group,
                    actualCount,
                    mExpectedCount);
        }

        @Override
        public String buildDescription() {
            return String.format("View has %d children", mExpectedCount);
        }
    }
}
