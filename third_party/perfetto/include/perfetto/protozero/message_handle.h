// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_PROTOZERO_MESSAGE_HANDLE_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_PROTOZERO_MESSAGE_HANDLE_H_

namespace protozero {

template <typename T> class MessageHandle {
public:
  MessageHandle() : MessageHandle(nullptr) {}
  explicit MessageHandle(T *message) : message_(message) {}

  explicit operator bool() const { return !!message_; }

  T &operator*() const { return *(operator->()); }

  T *operator->() const { return message_; }

  T *get() const { return message_; }

private:
  T *message_;
};

} // namespace protozero

#endif
