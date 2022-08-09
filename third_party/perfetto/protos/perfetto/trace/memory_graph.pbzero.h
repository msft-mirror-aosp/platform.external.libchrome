// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_MEMORY_GRAPH_PBZERO_H_
#define THIRD_PARTY_PERFETTO_PROTOS_PERFETTO_TRACE_MEMORY_GRAPH_PBZERO_H_

#include "third_party/perfetto/include/perfetto/protozero/message.h"

namespace perfetto {
namespace protos {
namespace pbzero {

class MemoryTrackerSnapshot_ProcessSnapshot;
class MemoryTrackerSnapshot_ProcessSnapshot_MemoryNode;
class MemoryTrackerSnapshot_ProcessSnapshot_MemoryEdge;
class MemoryTrackerSnapshot_ProcessSnapshot_MemoryNode_MemoryNodeEntry;

class MemoryTrackerSnapshot : public ::protozero::Message {
public:
  using ProcessSnapshot = MemoryTrackerSnapshot_ProcessSnapshot;
  template <typename T = MemoryTrackerSnapshot_ProcessSnapshot>
  T *add_process_memory_dumps() {
    return BeginNestedMessage<T>(0);
  }
};

class MemoryTrackerSnapshot_ProcessSnapshot : public ::protozero::Message {
public:
  using MemoryNode = MemoryTrackerSnapshot_ProcessSnapshot_MemoryNode;
  using MemoryEdge = MemoryTrackerSnapshot_ProcessSnapshot_MemoryEdge;
  void set_pid(int32_t) {}
  template <typename T = MemoryTrackerSnapshot_ProcessSnapshot_MemoryNode>
  T *add_allocator_dumps() {
    return BeginNestedMessage<T>(0);
  }
  template <typename T = MemoryTrackerSnapshot_ProcessSnapshot_MemoryEdge>
  T *add_memory_edges() {
    return BeginNestedMessage<T>(0);
  }
};

class MemoryTrackerSnapshot_ProcessSnapshot_MemoryNode
    : public ::protozero::Message {
public:
  using MemoryNodeEntry =
      MemoryTrackerSnapshot_ProcessSnapshot_MemoryNode_MemoryNodeEntry;
  void set_id(uint64_t) {}
  void set_absolute_name(std::string) {}
  void set_weak(bool) {}
  void set_size_bytes(uint64_t) {}
  template <
      typename T =
          MemoryTrackerSnapshot_ProcessSnapshot_MemoryNode_MemoryNodeEntry>
  T *add_entries() {
    return BeginNestedMessage<T>(0);
  }
};

class MemoryTrackerSnapshot_ProcessSnapshot_MemoryEdge
    : public ::protozero::Message {
public:
  void set_source_id(uint64_t) {}
  void set_target_id(uint64_t) {}
  void set_importance(uint32_t) {}
};

class MemoryTrackerSnapshot_ProcessSnapshot_MemoryNode_MemoryNodeEntry
    : public ::protozero::Message {
public:
  using Units = int32_t;
  static const Units UNSPECIFIED = 0;
  static const Units BYTES = 1;
  static const Units COUNT = 2;

  void set_name(std::string) {}
  void set_value_uint64(uint64_t) {}
  void set_value_string(std::string) {}
  void set_units(Units) {}
};

} // namespace pbzero
} // namespace protos
} // namespace perfetto

#endif
