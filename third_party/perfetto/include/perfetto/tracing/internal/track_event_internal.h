// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_

#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

#include <set>

namespace perfetto {
namespace internal {

struct TrackEventIncrementalState {
  std::set<uint64_t> seen_tracks;
};

} // namespace internal
} // namespace perfetto

#endif
