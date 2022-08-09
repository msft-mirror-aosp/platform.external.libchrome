// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACK_EVENT_TRACK_EVENT_PBZERO_H_
#define THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACK_EVENT_TRACK_EVENT_PBZERO_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_mojo_event_info.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"

namespace perfetto {
namespace protos {
namespace pbzero {

class TrackEvent : public ::protozero::Message {
public:
  void set_track_uuid(uint64_t) {}
  void set_source_location_iid(uint64_t) {}

  template <typename T = ChromeMojoEventInfo> T *set_chrome_mojo_event_info() {
    return BeginNestedMessage<T>(0);
  }

  template <typename T = TaskExecution> T *set_task_execution() {
    return BeginNestedMessage<T>(0);
  }

  template <typename T = LogMessage> T *set_log_message() {
    return BeginNestedMessage<T>(0);
  }

  template <typename T = SourceLocation> T *set_source_location() {
    return BeginNestedMessage<T>(0);
  }
};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
