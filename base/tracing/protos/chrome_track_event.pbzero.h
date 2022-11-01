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
class SequenceManagerTask;

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
  template <typename T = SequenceManagerTask>
  T *set_sequence_manager_task() {
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

class SequenceManagerTask : public ::protozero::Message {
public:
  enum Priority {
    UNKNOWN = 0,
    CONTROL_PRIORITY = 1,
    HIGHEST_PRIORITY = 2,
    VERY_HIGH_PRIORITY = 3,
    HIGH_PRIORITY = 4,
    NORMAL_PRIORITY = 5,
    LOW_PRIORITY = 6,
    BEST_EFFORT_PRIORITY = 7,
  };

  void set_priority(Priority) {}

  enum QueueName : int32_t {
    UNKNOWN_TQ = 0,
    DEFAULT_TQ = 1,
    TASK_ENVIRONMENT_DEFAULT_TQ = 2,
    TEST2_TQ = 3,
    TEST_TQ = 4,
    CONTROL_TQ = 5,
    SUBTHREAD_CONTROL_TQ = 6,
    SUBTHREAD_DEFAULT_TQ = 7,
    SUBTHREAD_INPUT_TQ = 8,
    UI_BEST_EFFORT_TQ = 9,
    UI_BOOTSTRAP_TQ = 10,
    UI_CONTROL_TQ = 11,
    UI_DEFAULT_TQ = 12,
    UI_NAVIGATION_NETWORK_RESPONSE_TQ = 13,
    UI_RUN_ALL_PENDING_TQ = 14,
    UI_SERVICE_WORKER_STORAGE_CONTROL_RESPONSE_TQ = 15,
    UI_THREAD_TQ = 16,
    UI_USER_BLOCKING_TQ = 17,
    UI_USER_INPUT_TQ = 18,
    UI_USER_VISIBLE_TQ = 19,
    IO_BEST_EFFORT_TQ = 20,
    IO_BOOTSTRAP_TQ = 21,
    IO_CONTROL_TQ = 22,
    IO_DEFAULT_TQ = 23,
    IO_NAVIGATION_NETWORK_RESPONSE_TQ = 24,
    IO_RUN_ALL_PENDING_TQ = 25,
    IO_SERVICE_WORKER_STORAGE_CONTROL_RESPONSE_TQ = 26,
    IO_THREAD_TQ = 27,
    IO_USER_BLOCKING_TQ = 28,
    IO_USER_INPUT_TQ = 29,
    IO_USER_VISIBLE_TQ = 30,
    COMPOSITOR_TQ = 31,
    DETACHED_TQ = 32,
    FRAME_DEFERRABLE_TQ = 33,
    FRAME_LOADING_CONTROL_TQ = 34,
    FRAME_LOADING_TQ = 35,
    FRAME_PAUSABLE_TQ = 36,
    FRAME_THROTTLEABLE_TQ = 37,
    FRAME_UNPAUSABLE_TQ = 38,
    IDLE_TQ = 39,
    INPUT_TQ = 40,
    IPC_TRACKING_FOR_CACHED_PAGES_TQ = 41,
    NON_WAKING_TQ = 42,
    OTHER_TQ = 43,
    V8_TQ = 44,
    WEB_SCHEDULING_TQ = 45,
    WORKER_IDLE_TQ = 46,
    WORKER_PAUSABLE_TQ = 47,
    WORKER_THREAD_INTERNAL_TQ = 48,
    WORKER_THROTTLEABLE_TQ = 49,
    WORKER_UNPAUSABLE_TQ = 50,
    WORKER_WEB_SCHEDULING_TQ = 51,
  };

  void set_queue_name(QueueName) {}

  static inline const char* QueueName_Name(QueueName) {
    return nullptr;
  }
};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
