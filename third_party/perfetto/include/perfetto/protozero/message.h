// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_PROTOZERO_MESSAGE_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_PROTOZERO_MESSAGE_H_

#define INCLUDE_PERFETTO_PROTOZERO_MESSAGE_H_

#include <memory>
#include <vector>

namespace protozero {

class Message {
public:
  Message() = default;

  uint32_t Finalize() {
    finalized_ = true;
    return 0;
  }

  bool is_finalized() { return finalized_; }

  template <class T> T *BeginNestedMessage(size_t) {
    nested_.emplace_back(new T());
    return static_cast<T*>(nested_.back().get());
  }

private:
  bool finalized_;
  std::vector<std::unique_ptr<Message>> nested_;
};

} // namespace protozero

#endif
