// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_INTERNED_DATA_INTERNED_DATA_PBZERO_H_
#define THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_INTERNED_DATA_INTERNED_DATA_PBZERO_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"

#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"

namespace perfetto {
namespace protos {
namespace pbzero {

class InternedData : public ::protozero::Message {
public:
  enum : int32_t {
    kSourceLocationsFieldNumber = 4,
    kUnsymbolizedSourceLocationsFieldNumber = 28,
    kLogMessageBodyFieldNumber = 20,
    kBuildIdsFieldNumber = 16,
    kMappingPathsFieldNumber = 17,
    kMappingsFieldNumber = 19,
  };

  template <typename T = SourceLocation> T *add_source_locations() {
    return BeginNestedMessage<T>(0);
  }

  template <typename T = LogMessageBody> T *add_log_message_body() {
    return BeginNestedMessage<T>(0);
  }
};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
