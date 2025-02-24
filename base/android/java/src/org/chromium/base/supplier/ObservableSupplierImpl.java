// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import android.os.Handler;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * Concrete implementation of {@link ObservableSupplier} to be used by classes owning the
 * ObservableSupplier and providing it as a dependency to others.
 *
 * <p>This class must only be accessed from a single thread.
 *
 * <pre>
 * To use:
 *   1. Create a new ObservableSupplierImpl<E> to pass as a dependency
 *   2. Call {@link #set(Object)} when the real object becomes available. {@link #set(Object)} may
 *      be called multiple times. Observers will be notified each time a new object is set.
 * </pre>
 *
 * @param <E> The type of the wrapped object.
 */
@NullMarked
public class ObservableSupplierImpl<E extends @Nullable Object> implements ObservableSupplier<E> {
    private final ThreadChecker mThreadChecker = new ThreadChecker();
    private final Handler mHandler = new Handler();

    private @Nullable E mObject;
    protected final ObserverList<Callback<E>> mObservers = new ObserverList<>();

    public ObservableSupplierImpl() {
        // Guard against creation on Instrumentation thread, since this is basically always a bug.
        assert !ThreadUtils.runningOnInstrumentationThread();
    }

    public ObservableSupplierImpl(E initialValue) {
        mObject = initialValue;
    }

    @Override
    public @Nullable E addObserver(Callback<@Nullable E> obs, @NotifyBehavior int behavior) {
        // ObserverList has its own ThreadChecker.
        mObservers.addObserver(obs);

        boolean notifyOnAdd = shouldNotifyOnAdd(behavior);
        boolean omitNullOnAdd = shouldOmitNullOnAdd(behavior);
        boolean postOnAdd = shouldPostOnAdd(behavior);
        boolean notify = notifyOnAdd && (mObject != null || !omitNullOnAdd);
        if (notify) {
            final E currentObject = mObject;
            if (postOnAdd) {
                mHandler.post(
                        () -> {
                            if (mObject == currentObject && mObservers.hasObserver(obs)) {
                                obs.onResult(mObject);
                            }
                        });
            } else {
                obs.onResult(mObject);
            }
        }

        return mObject;
    }

    @Override
    public void removeObserver(Callback<E> obs) {
        // ObserverList has its own ThreadChecker.
        mObservers.removeObserver(obs);
    }

    /**
     * Set the object supplied by this supplier. This will notify registered callbacks that the
     * dependency is available if the object changes. Object equality is used when deciding if the
     * object has changed, not reference equality.
     *
     * @param object The object to supply.
     */
    public void set(E object) {
        mThreadChecker.assertOnValidThread();
        if (Objects.equals(object, mObject)) {
            return;
        }

        mObject = object;

        for (Callback<E> observer : mObservers) {
            observer.onResult(object);
        }
    }

    @Override
    public @Nullable E get() {
        // Allow instrumentation thread access since tests often access variables for asserts.
        // https://crbug.com/1173814
        mThreadChecker.assertOnValidOrInstrumentationThread();
        return mObject;
    }

    /** Returns if there are any observers currently. */
    public boolean hasObservers() {
        // ObserverList has its own ThreadChecker.
        return !mObservers.isEmpty();
    }

    /**
     * Returns whether the observer should be notified on being added if {@link #mObject} is null.
     */
    private static boolean shouldOmitNullOnAdd(@NotifyBehavior int behavior) {
        return (NotifyBehavior.OMIT_NULL_ON_ADD & behavior) != 0;
    }

    /** Returns whether the observer should be notified on being added. */
    private static boolean shouldNotifyOnAdd(@NotifyBehavior int behavior) {
        return (NotifyBehavior.NOTIFY_ON_ADD & behavior) != 0;
    }

    /** Returns whether the observer should be notified asynchronously on being added. */
    private static boolean shouldPostOnAdd(int behavior) {
        return (NotifyBehavior.POST_ON_ADD & behavior) != 0;
    }
}
