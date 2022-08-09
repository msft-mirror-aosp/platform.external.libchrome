// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACK_EVENT_LOG_MESSAGE_PBZERO_H_
#define THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACK_EVENT_LOG_MESSAGE_PBZERO_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"

namespace perfetto {
namespace protos {
namespace pbzero {

class LogMessageBody : public ::protozero::Message {
public:
  void set_iid(uint64_t) {}
  void set_body(std::string) {}
};

class LogMessage : public ::protozero::Message {
public:
  void set_source_location_iid(uint64_t) {}
  void set_body_iid(uint64_t) {}
};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
