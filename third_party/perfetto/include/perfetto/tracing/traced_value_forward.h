// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_VALUE_FORWARD_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_VALUE_FORWARD_H_

namespace perfetto {

template <typename T, class = void> struct TraceFormatTraits;

template <typename T, typename ResultType = void, typename = void>
struct check_traced_value_support;

class TracedValue;
class TracedArray;
class TracedDictionary;

template <typename T>
    void WriteIntoTracedValue(TracedValue context, T&& value);

} // namespace perfetto

#endif
