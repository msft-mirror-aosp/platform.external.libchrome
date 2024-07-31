// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.annotation.Nullable;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ViewConditions.DisplayedCondition;
import org.chromium.base.test.transit.ViewConditions.NotDisplayedAnymoreCondition;
import org.chromium.base.test.transit.ViewElement.Scope;

/**
 * Represents a ViewElement added to a ConditionState.
 *
 * <p>ViewElements should be declared as constants, while ViewElementInStates are created by calling
 * {@link Elements.Builder#declareView(ViewElement)}.
 *
 * <p>Generates ENTER and EXIT Conditions for the ConditionalState to ensure the ViewElement is in
 * the right state.
 */
public class ViewElementInState extends ElementInState<View> {
    private final ViewElement mViewElement;

    ViewElementInState(ViewElement viewElement) {
        super(viewElement.getId());
        mViewElement = viewElement;
    }

    @Override
    public ConditionWithResult<View> createEnterCondition() {
        Matcher<View> viewMatcher = mViewElement.getViewMatcher();
        ViewElement.Options elementOptions = mViewElement.getOptions();
        DisplayedCondition.Options conditionOptions =
                DisplayedCondition.newOptions()
                        .withExpectEnabled(elementOptions.mExpectEnabled)
                        .withDisplayingAtLeast(elementOptions.mDisplayedPercentageRequired)
                        .build();
        return new DisplayedCondition(viewMatcher, conditionOptions);
    }

    @Override
    public @Nullable Condition createExitCondition() {
        switch (mViewElement.getScope()) {
            case Scope.SCOPED:
                return new NotDisplayedAnymoreCondition(mViewElement.getViewMatcher());
            case Scope.UNSCOPED:
                return null;
            default:
                assert false;
                return null;
        }
    }
}
