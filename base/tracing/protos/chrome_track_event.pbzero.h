// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BASE_TRACING_PROTOS_CHROME_TRACK_EVENT_PBZERO_H_
#define BASE_TRACING_PROTOS_CHROME_TRACK_EVENT_PBZERO_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {
namespace protos {
namespace pbzero {

class ChromeTaskPostedToDisabledQueue;
class ChromeThreadPoolTask;
class ChromeTaskAnnotator;
class ChromeMemoryPressureNotification;

enum MemoryPressureLevel {
  MEMORY_PRESSURE_LEVEL_NONE = 0,
  MEMORY_PRESSURE_LEVEL_MODERATE = 1,
  MEMORY_PRESSURE_LEVEL_CRITICAL = 2,
};

class ChromeTrackEvent : public ::perfetto::protos::pbzero::TrackEvent {
public:
  template <typename T = ChromeTaskPostedToDisabledQueue>
  T *set_chrome_task_posted_to_disabled_queue() {
    return BeginNestedMessage<T>(0);
  }
  template <typename T = ChromeThreadPoolTask> T *set_thread_pool_task() {
    return BeginNestedMessage<T>(0);
  }
  template <typename T = ChromeTaskAnnotator> T *set_chrome_task_annotator() {
    return BeginNestedMessage<T>(0);
  }
  template <typename T = ChromeMemoryPressureNotification>
  T *set_chrome_memory_pressure_notification() {
    return BeginNestedMessage<T>(0);
  }
};

class ChromeTaskPostedToDisabledQueue : public ::protozero::Message {
public:
  void set_task_queue_name(std::string) {}
  void set_time_since_disabled_ms(uint64_t) {}
  void set_ipc_hash(uint32_t) {}
  void set_source_location_iid(uint64_t) {}
};

class ChromeThreadPoolTask : public ::protozero::Message {
public:
  using Priority = int32_t;
  using ExecutionMode = int32_t;
  using ShutdownBehavior = int32_t;
  static const Priority PRIORITY_UNSPECIFIED = 0;
  static const Priority PRIORITY_BEST_EFFORT = 1;
  static const Priority PRIORITY_USER_VISIBLE = 2;
  static const Priority PRIORITY_USER_BLOCKING = 3;
  static const ExecutionMode EXECUTION_MODE_UNSPECIFIED = 0;
  static const ExecutionMode EXECUTION_MODE_PARALLEL = 1;
  static const ExecutionMode EXECUTION_MODE_SEQUENCED = 2;
  static const ExecutionMode EXECUTION_MODE_SINGLE_THREAD = 3;
  static const ExecutionMode EXECUTION_MODE_JOB = 4;
  static const ShutdownBehavior SHUTDOWN_BEHAVIOR_UNSPECIFIED = 0;
  static const ShutdownBehavior SHUTDOWN_BEHAVIOR_CONTINUE_ON_SHUTDOWN = 1;
  static const ShutdownBehavior SHUTDOWN_BEHAVIOR_SKIP_ON_SHUTDOWN = 2;
  static const ShutdownBehavior SHUTDOWN_BEHAVIOR_BLOCK_SHUTDOWN = 3;

  void set_task_priority(Priority) {}
  void set_execution_mode(ExecutionMode) {}
  void set_shutdown_behavior(ShutdownBehavior) {}
  void set_sequence_token(uint64_t) {}
};

class ChromeTaskAnnotator : public ::protozero::Message {
public:
  void set_ipc_hash(uint32_t) {}
  void set_task_delay_us(uint64_t) {}
};

class ChromeMemoryPressureNotification : public ::protozero::Message {
public:
  void set_level(::perfetto::protos::pbzero::MemoryPressureLevel) {}
  void set_creation_location_iid(uint64_t) {}
};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
