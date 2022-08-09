// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_VALUE_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_VALUE_H_

#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace perfetto {

class EventContext;

namespace protos {
namespace pbzero {
class DebugAnnotation;
}
} // namespace protos

namespace internal {
template <typename T> class has_traced_value_support {
public:
  static constexpr bool value = true;
};
} // namespace internal

template <typename T, class Result = void>
using check_traced_value_support_t = void;

class TracedValue {
public:
  TracedValue() = default;
  TracedValue(const TracedValue &) = delete;
  TracedValue &operator=(const TracedValue &) = delete;
  TracedValue &operator=(TracedValue &&) = delete;
  TracedValue(TracedValue &&) = default;

  void WriteInt64(int64_t) && {}
  void WriteUInt64(uint64_t) && {}
  void WriteDouble(double) && {}
  void WriteBoolean(bool) && {}
  void WriteString(const char *) && {}
  void WriteString(const char *, size_t len) && {}
  void WriteString(const std::string &) && {}
  void WritePointer(const void *) && {}

  TracedArray WriteArray() &&;
  TracedDictionary WriteDictionary() &&;
};

class TracedArray {
public:
  TracedArray(TracedValue) {}
  TracedArray(const TracedArray &) = delete;
  TracedArray &operator=(const TracedArray &) = delete;
  TracedArray &operator=(TracedArray &&) = delete;
  TracedArray(TracedArray &&) = default;

  template <typename T> void Append(T &&value) {}

private:
  TracedArray() = default;
  friend class TracedValue;
};

inline TracedArray TracedValue::WriteArray() && { return TracedArray(); }

class TracedDictionary {
public:
  TracedDictionary(TracedValue) {}
  TracedDictionary(const TracedDictionary &) = delete;
  TracedDictionary &operator=(const TracedDictionary &) = delete;
  TracedDictionary &operator=(TracedDictionary &&) = delete;
  TracedDictionary(TracedDictionary &&) = default;

  TracedValue AddItem(StringWrapper) { return TracedValue(); }

  template <typename T> void Add(StringWrapper, T &&) {}

private:
  TracedDictionary() = default;
  friend class TracedValue;
};

inline TracedDictionary TracedValue::WriteDictionary() && {
  return TracedDictionary();
}

template <typename T>
void WriteIntoTracedValue(TracedValue context, T &&value) {}

template <typename T>
void WriteIntoTracedValueWithFallback(TracedValue context, T &&value,
                                      const std::string &fallback) {}

namespace internal {
TracedValue CreateTracedValueFromProto(protos::pbzero::DebugAnnotation *,
                                       EventContext * = nullptr);
}

} // namespace perfetto

#endif
