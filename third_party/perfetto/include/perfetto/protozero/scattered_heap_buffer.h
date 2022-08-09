// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_PROTOZERO_SCATTERED_HEAP_BUFFER_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_PROTOZERO_SCATTERED_HEAP_BUFFER_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"

namespace protozero {

template <typename T = ::protozero::Message> class HeapBuffered {
public:
  HeapBuffered() : HeapBuffered(0, 0) {}
  HeapBuffered(int, int) {}

  // Disable copy and move.
  HeapBuffered(const HeapBuffered &) = delete;
  HeapBuffered &operator=(const HeapBuffered &) = delete;
  HeapBuffered(HeapBuffered &&) = delete;
  HeapBuffered &operator=(HeapBuffered &) = delete;

  T *get() { return msg_; }

private:
  T *msg_;
};

} // namespace protozero

#endif
