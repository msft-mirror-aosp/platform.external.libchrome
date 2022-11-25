// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACK_EVENT_SOURCE_LOCATION_PBZERO_H_
#define THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACK_EVENT_SOURCE_LOCATION_PBZERO_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"

namespace perfetto {
namespace protos {
namespace pbzero {

class SourceLocation : public ::protozero::Message {
public:
  void set_function_name(std::string) {}
  void set_file_name(std::string) {}
  void set_iid(uint64_t) {}
};

class UnsymbolizedSourceLocation : public ::protozero::Message {
 public:
  void set_iid(uint64_t value) {}
  void set_mapping_id(uint64_t value) {}
  void set_rel_pc(uint64_t value) {}
};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
