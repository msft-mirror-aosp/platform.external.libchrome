// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACK_EVENT_CHROME_MOJO_EVENT_INFO_PBZERO_H_
#define THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACK_EVENT_CHROME_MOJO_EVENT_INFO_PBZERO_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"

namespace perfetto {
namespace protos {
namespace pbzero {

class ChromeMojoEventInfo : public ::protozero::Message {
public:
  void set_ipc_hash(uint32_t) {}
  void set_mojo_interface_tag(std::string) {}
  void set_watcher_notify_interface_tag(std::string) {}
  void set_mojo_interface_method_iid(uint64_t) {}
};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
