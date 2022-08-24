// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_H_

#include <climits>
#include <cstdint>

#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

namespace partition_alloc::internal {

using FreeSlotBitmapCellType = uintptr_t;
constexpr size_t kFreeSlotBitmapBitsPerCell =
    sizeof(FreeSlotBitmapCellType) * CHAR_BIT;

// The number of bits necessary for the bitmap is equal to the maximum number of
// slots in a super page. We divide this by kBitsPerCell to get the number of
// cells in a bitmap.
constexpr size_t kFreeSlotBitmapSize = (kSuperPageSize / kAlignment) / CHAR_BIT;

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
ReservedFreeSlotBitmapSize() {
#if BUILDFLAG(USE_FREESLOT_BITMAP)
  return base::bits::AlignUp(kFreeSlotBitmapSize, PartitionPageSize());
#else
  return 0;
#endif
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
NumPartitionPagesPerFreeSlotBitmap() {
  return ReservedFreeSlotBitmapSize() / PartitionPageSize();
}

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_H_