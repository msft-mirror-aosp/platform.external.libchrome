// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/event_name_filter.h"

#include "base/trace_event/trace_event_impl.h"

namespace base {
namespace trace_event {

// static
const char EventNameFilter::kName[] = "event_whitelist_predicate";

EventNameFilter::EventNameFilter(
    std::unique_ptr<EventNamesAllowlist> event_names_allowlist)
    : event_names_allowlist_(std::move(event_names_allowlist)) {}

EventNameFilter::~EventNameFilter() = default;

bool EventNameFilter::FilterTraceEvent(const TraceEvent& trace_event) const {
  return event_names_allowlist_->count(trace_event.name()) != 0;
}

}  // namespace trace_event
}  // namespace base
