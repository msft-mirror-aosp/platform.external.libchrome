// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Indirection header that allows callers to use
// `PartitionFreelistEntry` without regard for the implementation.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_

#include <cstddef>

#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

namespace partition_alloc::internal {

[[noreturn]] PA_NOINLINE PA_COMPONENT_EXPORT(
    PARTITION_ALLOC) void FreelistCorruptionDetected(size_t slot_size);

}  // namespace partition_alloc::internal

#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
#include "base/allocator/partition_allocator/pool_offset_freelist.h"  // IWYU pragma: export
#else
#include "base/allocator/partition_allocator/encoded_freelist.h"  // IWYU pragma: export
#endif  // BUILDFLAG(USE_FREELIST_POOL_OFFSETS)

namespace partition_alloc::internal {

// Assertions that are agnostic to the implementation of the freelist.

static_assert(kSmallestBucket >= sizeof(PartitionFreelistEntry),
              "Need enough space for freelist entries in the smallest slot");

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
// The smallest bucket actually used. Note that the smallest request is 1 (if
// it's 0, it gets patched to 1), and ref-count gets added to it.
namespace {
constexpr size_t kSmallestUsedBucket =
    base::bits::AlignUp(1 + sizeof(PartitionRefCount), kSmallestBucket);
}
static_assert(kSmallestUsedBucket >=
                  sizeof(PartitionFreelistEntry) + sizeof(PartitionRefCount),
              "Need enough space for freelist entries and the ref-count in the "
              "smallest *used* slot");
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
