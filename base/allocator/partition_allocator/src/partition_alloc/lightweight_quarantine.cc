// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/lightweight_quarantine.h"

#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"

namespace partition_alloc::internal {

LightweightQuarantineBranch LightweightQuarantineRoot::CreateBranch(
    bool lock_required) {
  return LightweightQuarantineBranch(*this, lock_required);
}

LightweightQuarantineBranch::LightweightQuarantineBranch(Root& root,
                                                         bool lock_required)
    : root_(root), lock_required_(lock_required) {}

LightweightQuarantineBranch::LightweightQuarantineBranch(
    LightweightQuarantineBranch&& b)
    : root_(b.root_),
      lock_required_(b.lock_required_),
      slots_(std::move(b.slots_)),
      branch_size_in_bytes_(b.branch_size_in_bytes_) {
  b.branch_size_in_bytes_ = 0;
}

LightweightQuarantineBranch::~LightweightQuarantineBranch() {
  Purge();
  slots_.clear();
}

bool LightweightQuarantineBranch::Quarantine(void* object,
                                             SlotSpanMetadata* slot_span,
                                             uintptr_t slot_start) {
  const auto usable_size = root_.allocator_root_.GetSlotUsableSize(slot_span);

  const size_t capacity_in_bytes =
      root_.capacity_in_bytes_.load(std::memory_order_relaxed);

  {
    ConditionalScopedGuard guard(lock_required_, lock_);

    const size_t size_in_bytes_held_by_others =
        root_.size_in_bytes_.load(std::memory_order_relaxed) -
        branch_size_in_bytes_;
    if (capacity_in_bytes < size_in_bytes_held_by_others + usable_size) {
      // Even if this branch dequarantines all entries held by it, this entry
      // cannot fit within the capacity.
      root_.allocator_root_.FreeNoHooksImmediate(object, slot_span, slot_start);
      root_.quarantine_miss_count_.fetch_add(1u, std::memory_order_relaxed);
      return false;
    }

    // Dequarantine some entries as required.
    PurgeInternal(capacity_in_bytes - usable_size);

    // Update stats (locked).
    branch_size_in_bytes_ += usable_size;
    PA_DCHECK(branch_size_in_bytes_ <= capacity_in_bytes);

    slots_.emplace_back(object, usable_size);

    // Swap randomly so that the quarantine list remain shuffled.
    // This is not uniformly random, but sufficiently random.
    const size_t random_index = random_.RandUint32() % slots_.size();
    std::swap(slots_[random_index], slots_.back());
  }

  // Update stats (not locked).
  root_.count_.fetch_add(1, std::memory_order_relaxed);
  root_.size_in_bytes_.fetch_add(usable_size, std::memory_order_relaxed);
  root_.cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  root_.cumulative_size_in_bytes_.fetch_add(usable_size,
                                            std::memory_order_relaxed);
  return true;
}

void LightweightQuarantineBranch::PurgeInternal(size_t target_size_in_bytes) {
  size_t size_in_bytes = root_.size_in_bytes_.load(std::memory_order_acquire);
  int64_t freed_count = 0;
  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (!slots_.empty() && target_size_in_bytes < size_in_bytes) {
    // As quarantined entries are shuffled, picking last entry is equivalent
    // to picking random entry.
    const auto& to_free = slots_.back();
    size_t to_free_size = to_free.usable_size;

    auto* slot_span = SlotSpanMetadata::FromObject(to_free.object);
    uintptr_t slot_start =
        root_.allocator_root_.ObjectToSlotStart(to_free.object);
    PA_DCHECK(slot_span == SlotSpanMetadata::FromSlotStart(slot_start));

    PA_DCHECK(to_free.object);
    root_.allocator_root_.FreeNoHooksImmediate(to_free.object, slot_span,
                                               slot_start);

    freed_count++;
    freed_size_in_bytes += to_free_size;
    size_in_bytes -= to_free_size;

    slots_.pop_back();
  }

  branch_size_in_bytes_ -= freed_size_in_bytes;
  root_.count_.fetch_sub(freed_count, std::memory_order_relaxed);
  root_.size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                 std::memory_order_release);
}

}  // namespace partition_alloc::internal
