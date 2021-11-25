// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#include "base/allocator/buildflags.h"

// USE_BACKUP_REF_PTR implies USE_PARTITION_ALLOC, needed for code under
// allocator/partition_allocator/ to be built.
#if BUILDFLAG(USE_BACKUP_REF_PTR)

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"

namespace base {

namespace internal {

void BackupRefPtrImpl::AcquireInternal(const volatile void* cv_ptr) {
  // |const volatile| qualifiers are used only to compile with |T*| pointers
  // passed by the caller that may have those qualifiers. From now on, the
  // pointer value is used, but is never dereferenced.
  //
  // TODO(bartekn): Convert to |uintptr_t address|, incl. callees.
  void* ptr = const_cast<void*>(cv_ptr);
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CHECK(IsManagedByPartitionAllocBRPPool(ptr));
#endif
  void* slot_start = PartitionAllocGetSlotStartInBRPPool(ptr);
  PartitionRefCountPointer(slot_start)->Acquire();
}

void BackupRefPtrImpl::ReleaseInternal(const volatile void* cv_ptr) {
  // |const volatile| qualifiers are used only to compile with |T*| pointers
  // passed by the caller that may have those qualifiers. From now on, the
  // pointer value is used, but is never dereferenced.
  //
  // TODO(bartekn): Convert to |uintptr_t address|, incl. callees.
  void* ptr = const_cast<void*>(cv_ptr);
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CHECK(IsManagedByPartitionAllocBRPPool(ptr));
#endif
  void* slot_start = PartitionAllocGetSlotStartInBRPPool(ptr);
  if (PartitionRefCountPointer(slot_start)->Release())
    PartitionAllocFreeForRefCounting(slot_start);
}

bool BackupRefPtrImpl::IsPointeeAlive(const volatile void* cv_ptr) {
  // |const volatile| qualifiers are used only to compile with |T*| pointers
  // passed by the caller that may have those qualifiers. From now on, the
  // pointer value is used, but is never dereferenced.
  //
  // TODO(bartekn): Convert to |uintptr_t address|, incl. callees.
  void* ptr = const_cast<void*>(cv_ptr);
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  CHECK(IsManagedByPartitionAllocBRPPool(ptr));
#endif
  void* slot_start = PartitionAllocGetSlotStartInBRPPool(ptr);
  return PartitionRefCountPointer(slot_start)->IsAlive();
}

bool BackupRefPtrImpl::IsValidDelta(const volatile void* cv_ptr,
                                    ptrdiff_t delta_in_bytes) {
  // |const volatile| qualifiers are used only to compile with |T*| pointers
  // passed by the caller that may have those qualifiers. From now on, the
  // pointer value is used, but is never dereferenced.
  //
  // TODO(bartekn): Convert to |uintptr_t address|, incl. callees.
  void* ptr = const_cast<void*>(cv_ptr);
  return PartitionAllocIsValidPtrDelta(ptr, delta_in_bytes);
}

#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
void CheckThatAddressIsntWithinFirstPartitionPage(void* ptr) {
  if (IsManagedByDirectMap(ptr)) {
    uintptr_t reservation_start = GetDirectMapReservationStart(ptr);
    CHECK(reinterpret_cast<uintptr_t>(ptr) - reservation_start >=
          PartitionPageSize());
  } else {
    CHECK(IsManagedByNormalBuckets(ptr));
    CHECK(reinterpret_cast<uintptr_t>(ptr) % kSuperPageSize >=
          PartitionPageSize());
  }
}
#endif  // DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)

}  // namespace internal

}  // namespace base

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
