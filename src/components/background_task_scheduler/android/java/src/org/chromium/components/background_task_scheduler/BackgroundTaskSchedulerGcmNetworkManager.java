// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.OneoffTask;
import com.google.android.gms.gcm.PeriodicTask;
import com.google.android.gms.gcm.Task;
import com.google.android.gms.gcm.TaskParams;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.util.concurrent.TimeUnit;

/**
 * An implementation of {@link BackgroundTaskSchedulerDelegate} that uses the Play Services
 * {@link GcmNetworkManager} to schedule jobs.
 */
class BackgroundTaskSchedulerGcmNetworkManager implements BackgroundTaskSchedulerDelegate {
    private static final String TAG = "BkgrdTaskSchedGcmNM";

    /** Delta time for expiration checks, after the end time. */
    static final long DEADLINE_DELTA_MS = 1000;

    /** Clock to use so we can mock time in tests. */
    public interface Clock { long currentTimeMillis(); }

    private static Clock sClock = System::currentTimeMillis;

    @VisibleForTesting
    static void setClockForTesting(Clock clock) {
        sClock = clock;
    }

    static Long getDeadlineTimeFromTaskParams(@NonNull TaskParams taskParams) {
        Bundle extras = taskParams.getExtras();
        if (extras == null || !extras.containsKey(BACKGROUND_TASK_DEADLINE_KEY)) {
            return null;
        }
        return extras.getLong(BACKGROUND_TASK_DEADLINE_KEY);
    }

    private static long getDeadlineTime(TaskInfo taskInfo) {
        long windowEndTimeMs = taskInfo.getOneOffInfo().getWindowEndTimeMs();
        return sClock.currentTimeMillis() + windowEndTimeMs;
    }

    /**
     * Retrieves the {@link TaskParameters} from the {@link TaskParams}, which are passed as
     * one of the keys. Only values valid for {@link android.os.BaseBundle} are supported, and other
     * values are stripped at the time when the task is scheduled.
     *
     * @param taskParams the {@link TaskParams} to extract the {@link TaskParameters} from.
     * @return the {@link TaskParameters} for the current job.
     */
    static TaskParameters getTaskParametersFromTaskParams(@NonNull TaskParams taskParams) {
        int taskId;
        try {
            taskId = Integer.parseInt(taskParams.getTag());
        } catch (NumberFormatException e) {
            Log.e(TAG, "Cound not parse task ID from task tag: " + taskParams.getTag());
            return null;
        }

        TaskParameters.Builder builder = TaskParameters.create(taskId);

        Bundle extras = taskParams.getExtras();
        Bundle taskExtras = extras.getBundle(BACKGROUND_TASK_EXTRAS_KEY);
        builder.addExtras(taskExtras);

        return builder.build();
    }

    @VisibleForTesting
    static Task createTaskFromTaskInfo(@NonNull TaskInfo taskInfo) {
        Bundle taskExtras = new Bundle();
        taskExtras.putBundle(BACKGROUND_TASK_EXTRAS_KEY, taskInfo.getExtras());

        TaskBuilderVisitor taskBuilderVisitor = new TaskBuilderVisitor(taskExtras);
        taskInfo.getTimingInfo().accept(taskBuilderVisitor);
        Task.Builder builder = taskBuilderVisitor.getBuilder();

        builder.setPersisted(taskInfo.isPersisted())
                .setRequiredNetwork(getGcmNetworkManagerNetworkTypeFromTypeFromTaskNetworkType(
                        taskInfo.getRequiredNetworkType()))
                .setRequiresCharging(taskInfo.requiresCharging())
                .setService(BackgroundTaskGcmTaskService.class)
                .setTag(taskIdToTaskTag(taskInfo.getTaskId()))
                .setUpdateCurrent(taskInfo.shouldUpdateCurrent());

        return builder.build();
    }

    private static class TaskBuilderVisitor implements TaskInfo.TimingInfoVisitor {
        private Task.Builder mBuilder;
        private final Bundle mTaskExtras;

        TaskBuilderVisitor(Bundle taskExtras) {
            mTaskExtras = taskExtras;
        }

        // Only valid after a TimingInfo object was visited.
        Task.Builder getBuilder() {
            return mBuilder;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            if (oneOffInfo.expiresAfterWindowEndTime()) {
                mTaskExtras.putLong(BACKGROUND_TASK_DEADLINE_KEY,
                        sClock.currentTimeMillis() + oneOffInfo.getWindowEndTimeMs());
            }

            OneoffTask.Builder builder = new OneoffTask.Builder();
            long windowStartSeconds = oneOffInfo.hasWindowStartTimeConstraint()
                    ? TimeUnit.MILLISECONDS.toSeconds(oneOffInfo.getWindowStartTimeMs())
                    : 0;
            long windowEndTimeMs = oneOffInfo.getWindowEndTimeMs();
            if (oneOffInfo.expiresAfterWindowEndTime()) {
                windowEndTimeMs += DEADLINE_DELTA_MS;
            }
            builder.setExecutionWindow(
                    windowStartSeconds, TimeUnit.MILLISECONDS.toSeconds(windowEndTimeMs));
            builder.setExtras(mTaskExtras);
            mBuilder = builder;
        }

        @Override
        public void visit(TaskInfo.PeriodicInfo periodicInfo) {
            PeriodicTask.Builder builder = new PeriodicTask.Builder();
            builder.setPeriod(TimeUnit.MILLISECONDS.toSeconds(periodicInfo.getIntervalMs()));
            if (periodicInfo.hasFlex()) {
                builder.setFlex(TimeUnit.MILLISECONDS.toSeconds(periodicInfo.getFlexMs()));
            }
            mBuilder = builder;
        }
    }

    private static int getGcmNetworkManagerNetworkTypeFromTypeFromTaskNetworkType(
            @TaskInfo.NetworkType int networkType) {
        switch (networkType) {
            // This is correct: GcmNM ANY means no network is guaranteed.
            case TaskInfo.NetworkType.NONE:
                return Task.NETWORK_STATE_ANY;
            case TaskInfo.NetworkType.ANY:
                return Task.NETWORK_STATE_CONNECTED;
            case TaskInfo.NetworkType.UNMETERED:
                return Task.NETWORK_STATE_UNMETERED;
            default:
                assert false;
        }
        return Task.NETWORK_STATE_ANY;
    }

    @Override
    public boolean schedule(Context context, @NonNull TaskInfo taskInfo) {
        ThreadUtils.assertOnUiThread();

        GcmNetworkManager gcmNetworkManager = getGcmNetworkManager(context);
        if (gcmNetworkManager == null) {
            Log.e(TAG, "GcmNetworkManager is not available.");
            return false;
        }

        try {
            Task task = createTaskFromTaskInfo(taskInfo);
            gcmNetworkManager.schedule(task);
        } catch (IllegalArgumentException e) {
            String gcmErrorMessage = e.getMessage() == null ? "null." : e.getMessage();
            Log.e(TAG,
                    "GcmNetworkManager failed to schedule task, gcm message: " + gcmErrorMessage);
            return false;
        }

        return true;
    }

    @Override
    public void cancel(Context context, int taskId) {
        ThreadUtils.assertOnUiThread();

        GcmNetworkManager gcmNetworkManager = getGcmNetworkManager(context);
        if (gcmNetworkManager == null) {
            Log.e(TAG, "GcmNetworkManager is not available.");
            return;
        }

        try {
            gcmNetworkManager.cancelTask(
                    taskIdToTaskTag(taskId), BackgroundTaskGcmTaskService.class);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "GcmNetworkManager failed to cancel task.");
        }
    }

    private GcmNetworkManager getGcmNetworkManager(Context context) {
        if (GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(context)
                == ConnectionResult.SUCCESS) {
            return GcmNetworkManager.getInstance(context);
        }
        return null;
    }

    private static String taskIdToTaskTag(int taskId) {
        return Integer.toString(taskId);
    }
}
