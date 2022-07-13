// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACK_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACK_H_

#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

struct Track {
  constexpr Track() : uuid(0), parent_uuid(0) {}
  constexpr Track(uint64_t id, Track parent = Track())
      : uuid(id ^ parent.uuid), parent_uuid(parent.uuid) {}
  static Track FromPointer(const void* ptr, Track parent = Track()) {
    return Track();
  }
  const uint64_t uuid;
  const uint64_t parent_uuid;
};

struct ThreadTrack : public Track {
  static ThreadTrack Current() { return ThreadTrack(); }
  static ThreadTrack ForThread(base::PlatformThreadId tid_) {
    return ThreadTrack();
  }

private:
  ThreadTrack() = default;
};

namespace internal {
class TrackRegistry {
public:
  TrackRegistry() = default;
  ~TrackRegistry() = default;

  static TrackRegistry *Get() { return instance_; }

  template <typename TrackType>
  void
  SerializeTrack(const TrackType &track,
                 protozero::MessageHandle<protos::pbzero::TracePacket> packet) {
  }

private:
  static TrackRegistry *instance_;
};
} // namespace internal

} // namespace perfetto

#endif
