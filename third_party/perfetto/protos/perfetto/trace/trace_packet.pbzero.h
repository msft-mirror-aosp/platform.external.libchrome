// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACE_PACKET_PBZERO_H_
#define THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_TRACE_PACKET_PBZERO_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"

namespace perfetto {
namespace protos {
namespace pbzero {

class TracePacket : public ::protozero::Message {};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
