// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACK_EVENT_INTERNED_DATA_INDEX_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACK_EVENT_INTERNED_DATA_INDEX_H_

namespace perfetto {

class EventContext;

template <typename InternedDataType, size_t FieldNumber, typename ValueType,
          typename FakeTraits = void>
class TrackEventInternedDataIndex {
public:
  template <typename... Args>
  static size_t Get(EventContext *, const ValueType &value,
                    Args &&...add_args) {
    return 0;
  }
};

} // namespace perfetto

#endif
