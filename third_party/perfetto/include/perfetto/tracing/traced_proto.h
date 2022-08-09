// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_PROTO_H_
#define THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_PROTO_H_

namespace perfetto {

template <typename MessageType>
class TracedProto {
};

template <typename MessageType, typename ValueType>
static void WriteIntoTracedProto(TracedProto<MessageType>, ValueType&& value) {
}

}


#endif  // THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_PROTO_H_
