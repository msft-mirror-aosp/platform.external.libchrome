// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.util.Pair;

import org.chromium.base.TraceEvent;

import java.util.LinkedList;

import javax.annotation.concurrent.GuardedBy;

/**
 * Allows chaining multiple tasks on arbitrary threads, with the next task posted when one
 * completes.
 *
 * Threading:
 * - This class is threadsafe and all methods may be called from any thread.
 * - Tasks may run with arbitrary TaskTraits, unless tasks are coalesced, in which case all tasks
 *   must run on the same thread.
 */
public class ChainedTasks {
    private LinkedList<Pair<TaskTraits, Runnable>> mTasks = new LinkedList<>();
    @GuardedBy("mTasks")
    private boolean mFinalized;
    private volatile boolean mCanceled;

    private final Runnable mRunAndPost = new Runnable() {
        @Override
        @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
        public void run() {
            if (mCanceled) return;

            Pair<TaskTraits, Runnable> pair = mTasks.pop();
            try (TraceEvent e = TraceEvent.scoped(
                         "ChainedTask.run: " + pair.second.getClass().getName())) {
                pair.second.run();
            }
            if (!mTasks.isEmpty()) PostTask.postTask(mTasks.peek().first, this);
        }
    };

    /**
     * Adds a task to the list of tasks to run. Cannot be called once {@link start()} has been
     * called.
     */
    public void add(TaskTraits traits, Runnable task) {
        synchronized (mTasks) {
            if (mFinalized) throw new IllegalStateException("Must not call add() after start()");
            mTasks.add(new Pair<>(traits, task));
        }
    }

    /**
     * Cancels the remaining tasks.
     */
    public void cancel() {
        synchronized (mTasks) {
            mFinalized = true;
            mCanceled = true;
        }
    }

    /**
     * Posts or runs all the tasks, one by one.
     *
     * @param coalesceTasks if false, posts the tasks. Otherwise run them in a single task. If
     * called on the thread matching the TaskTraits, will block and run all tasks synchronously.
     */
    public void start(final boolean coalesceTasks) {
        synchronized (mTasks) {
            if (mFinalized) throw new IllegalStateException("Cannot call start() several times");
            mFinalized = true;
        }
        if (mTasks.isEmpty()) return;
        if (coalesceTasks) {
            TaskTraits traits = mTasks.peek().first;
            PostTask.runOrPostTask(traits, () -> {
                for (Pair<TaskTraits, Runnable> pair : mTasks) {
                    assert PostTask.canRunTaskImmediately(pair.first);
                    pair.second.run();
                    if (mCanceled) return;
                }
            });
        } else {
            PostTask.postTask(mTasks.peek().first, mRunAndPost);
        }
    }
}
