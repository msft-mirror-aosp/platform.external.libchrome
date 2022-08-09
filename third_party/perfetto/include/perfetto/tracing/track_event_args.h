// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACK_EVENT_ARGS_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACK_EVENT_ARGS_H_

#include "third_party/perfetto/include/perfetto/tracing/event_context.h"

namespace perfetto {

class Flow {
public:
  static inline std::function<void(EventContext &)>
  ProcessScoped(uint64_t flow_id) {
    return [](perfetto::EventContext &ctx) {};
  }

  static inline std::function<void(EventContext &)> Global(uint64_t flow_id) {
    return [](perfetto::EventContext &ctx) {};
  }
};

class TerminatingFlow {
public:
  static inline std::function<void(EventContext &)>
  ProcessScoped(uint64_t flow_id) {
    return [](perfetto::EventContext &ctx) {};
  }
};

} // namespace perfetto

#endif
