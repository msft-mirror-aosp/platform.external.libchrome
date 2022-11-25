// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_EVENT_CONTEXT_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_EVENT_CONTEXT_H_

#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

class EventContext {
public:
  EventContext(EventContext &&) = default;
  explicit EventContext(
      protos::pbzero::TrackEvent *event,
      internal::TrackEventIncrementalState *incremental_state = nullptr,
      bool filter_debug_annotations = false)
      : event_(event),
        filter_debug_annotations_(filter_debug_annotations),
        incremental_state_(incremental_state) {}

  bool ShouldFilterDebugAnnotations() const {
    return filter_debug_annotations_;
  }

  template <typename EventType = protos::pbzero::TrackEvent>
  EventType *event() const {
    static_assert(
        sizeof(EventType) == sizeof(protos::pbzero::TrackEvent),
        "Event type must be binary compatible with protos::pbzero::TrackEvent");
    return static_cast<EventType *>(event_);
  }

  internal::TrackEventIncrementalState* GetIncrementalState() const {
    return incremental_state_;
  }

private:
  protos::pbzero::TrackEvent *event_;
  const bool filter_debug_annotations_;
  internal::TrackEventIncrementalState* incremental_state_;
};

} // namespace perfetto

#endif
