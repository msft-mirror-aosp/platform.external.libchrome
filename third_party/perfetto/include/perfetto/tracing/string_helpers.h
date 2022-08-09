// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_STRING_HELPERS_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_STRING_HELPERS_H_

namespace perfetto {

class StringWrapper {
public:
  template <typename T> constexpr StringWrapper(T) {}

  const char value[1] = "";
};

using StaticString = StringWrapper;
using DynamicString = StringWrapper;

} // namespace perfetto

#endif
