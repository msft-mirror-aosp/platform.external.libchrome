// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_INTERNAL_WRITE_TRACK_EVENT_ARGS_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_INTERNAL_WRITE_TRACK_EVENT_ARGS_H_

#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace perfetto {
namespace internal {

template <typename... Args>
void WriteTrackEventArgs(EventContext event_ctx, Args &&...args) {}

} // namespace internal
} // namespace perfetto

#endif
