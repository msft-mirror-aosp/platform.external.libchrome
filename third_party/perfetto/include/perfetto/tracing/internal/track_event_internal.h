// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_

#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

#include <set>

namespace perfetto {
namespace internal {

class BaseTrackEventInternedDataIndex {
 public:
  virtual ~BaseTrackEventInternedDataIndex();
};


struct TrackEventIncrementalState {
  static constexpr size_t kMaxInternedDataFields = 32;
  
  std::set<uint64_t> seen_tracks;
  protozero::HeapBuffered<protos::pbzero::InternedData>
      serialized_interned_data;

  using InternedDataIndex =
      std::pair</* interned_data.proto field number */ size_t,
                std::unique_ptr<BaseTrackEventInternedDataIndex>>;
  std::array<InternedDataIndex, kMaxInternedDataFields> interned_data_indices =
      {};
};

} // namespace internal
} // namespace perfetto

#endif
