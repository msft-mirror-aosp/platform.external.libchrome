// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jank_metric_uma_recorder.h"

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/base_jni/JankMetricUMARecorder_jni.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace base::android {

namespace {

void RecordJankMetricReportingIntervalTraceEvent(
    int64_t reporting_interval_start_time,
    int64_t reporting_interval_duration,
    uint64_t janky_frame_count,
    uint64_t non_janky_frame_count,
    int scenario) {
  if (reporting_interval_start_time <= 0) {
    return;
  }

  // The following code does nothing if base tracing is disabled.
  [[maybe_unused]] auto t = perfetto::Track(
      static_cast<uint64_t>(reporting_interval_start_time + scenario));
  TRACE_EVENT_BEGIN(
      "android_webview.timeline,android.ui.jank",
      "JankMetricsReportingInterval", t,
      base::TimeTicks::FromUptimeMillis(reporting_interval_start_time),
      "janky_frames", janky_frame_count, "non_janky_frames",
      non_janky_frame_count, "scenario", scenario);
  TRACE_EVENT_END(
      "android_webview.timeline,android.ui.jank", t,
      base::TimeTicks::FromUptimeMillis(
          (reporting_interval_start_time + reporting_interval_duration)));
}

}  // namespace

const char* GetAndroidFrameTimelineJankHistogramName(JankScenario scenario) {
#define HISTOGRAM_NAME(x) "Android.FrameTimelineJank.FrameJankStatus." #x
  switch (scenario) {
    case JankScenario::PERIODIC_REPORTING:
      return HISTOGRAM_NAME(Total);
    case JankScenario::OMNIBOX_FOCUS:
      return HISTOGRAM_NAME(OmniboxFocus);
    case JankScenario::NEW_TAB_PAGE:
      return HISTOGRAM_NAME(NewTabPage);
    case JankScenario::STARTUP:
      return HISTOGRAM_NAME(Startup);
    case JankScenario::TAB_SWITCHER:
      return HISTOGRAM_NAME(TabSwitcher);
    case JankScenario::OPEN_LINK_IN_NEW_TAB:
      return HISTOGRAM_NAME(OpenLinkInNewTab);
    case JankScenario::START_SURFACE_HOMEPAGE:
      return HISTOGRAM_NAME(StartSurfaceHomepage);
    case JankScenario::START_SURFACE_TAB_SWITCHER:
      return HISTOGRAM_NAME(StartSurfaceTabSwitcher);
    case JankScenario::FEED_SCROLLING:
      return HISTOGRAM_NAME(FeedScrolling);
    case JankScenario::WEBVIEW_SCROLLING:
      return HISTOGRAM_NAME(WebviewScrolling);
    default:
      return HISTOGRAM_NAME(UNKNOWN);
  }
#undef HISTOGRAM_NAME
}

const char* GetAndroidFrameTimelineDurationHistogramName(
    JankScenario scenario) {
#define HISTOGRAM_NAME(x) "Android.FrameTimelineJank.Duration." #x
  switch (scenario) {
    case JankScenario::PERIODIC_REPORTING:
      return HISTOGRAM_NAME(Total);
    case JankScenario::OMNIBOX_FOCUS:
      return HISTOGRAM_NAME(OmniboxFocus);
    case JankScenario::NEW_TAB_PAGE:
      return HISTOGRAM_NAME(NewTabPage);
    case JankScenario::STARTUP:
      return HISTOGRAM_NAME(Startup);
    case JankScenario::TAB_SWITCHER:
      return HISTOGRAM_NAME(TabSwitcher);
    case JankScenario::OPEN_LINK_IN_NEW_TAB:
      return HISTOGRAM_NAME(OpenLinkInNewTab);
    case JankScenario::START_SURFACE_HOMEPAGE:
      return HISTOGRAM_NAME(StartSurfaceHomepage);
    case JankScenario::START_SURFACE_TAB_SWITCHER:
      return HISTOGRAM_NAME(StartSurfaceTabSwitcher);
    case JankScenario::FEED_SCROLLING:
      return HISTOGRAM_NAME(FeedScrolling);
    case JankScenario::WEBVIEW_SCROLLING:
      return HISTOGRAM_NAME(WebviewScrolling);
    default:
      return HISTOGRAM_NAME(UNKNOWN);
  }
#undef HISTOGRAM_NAME
}

// This function is called from Java with JNI, it's declared in
// base/base_jni/JankMetricUMARecorder_jni.h which is an autogenerated
// header. The actual implementation is in RecordJankMetrics for simpler
// testing.
void JNI_JankMetricUMARecorder_RecordJankMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jlongArray>& java_durations_ns,
    const base::android::JavaParamRef<jbooleanArray>& java_jank_status,
    jlong java_reporting_interval_start_time,
    jlong java_reporting_interval_duration,
    jint java_scenario_enum) {
  RecordJankMetrics(env, java_durations_ns, java_jank_status,
                    java_reporting_interval_start_time,
                    java_reporting_interval_duration, java_scenario_enum);
}

void RecordJankMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jlongArray>& java_durations_ns,
    const base::android::JavaParamRef<jbooleanArray>& java_jank_status,
    jlong java_reporting_interval_start_time,
    jlong java_reporting_interval_duration,
    jint java_scenario_enum) {
  std::vector<int64_t> durations_ns;
  JavaLongArrayToInt64Vector(env, java_durations_ns, &durations_ns);

  std::vector<bool> jank_status;
  JavaBooleanArrayToBoolVector(env, java_jank_status, &jank_status);

  JankScenario scenario = static_cast<JankScenario>(java_scenario_enum);

  const char* frame_duration_histogram_name =
      GetAndroidFrameTimelineDurationHistogramName(scenario);
  const char* janky_frames_per_scenario_histogram_name =
      GetAndroidFrameTimelineJankHistogramName(scenario);

  for (const int64_t frame_duration_ns : durations_ns) {
    base::UmaHistogramTimes(frame_duration_histogram_name,
                            base::Nanoseconds(frame_duration_ns));
  }

  uint64_t janky_frame_count = 0;

  for (bool is_janky : jank_status) {
    base::UmaHistogramEnumeration(
        janky_frames_per_scenario_histogram_name,
        is_janky ? FrameJankStatus::kJanky : FrameJankStatus::kNonJanky);
    if (is_janky) {
      ++janky_frame_count;
    }
  }

  RecordJankMetricReportingIntervalTraceEvent(
      java_reporting_interval_start_time, java_reporting_interval_duration,
      janky_frame_count, jank_status.size() - janky_frame_count,
      java_scenario_enum);
}

}  // namespace base::android
