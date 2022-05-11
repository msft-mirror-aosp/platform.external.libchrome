// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STARSCAN_FWD_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STARSCAN_FWD_H_

#include <cstdint>

namespace partition_alloc::internal {

// Defines what thread executes a StarScan task.
enum class Context {
  // For tasks executed from mutator threads (safepoints).
  kMutator,
  // For concurrent scanner tasks.
  kScanner
};

// Defines ISA extension for scanning.
enum class SimdSupport : uint8_t {
  kUnvectorized,
  kSSE41,
  kAVX2,
  kNEON,
};

}  // namespace partition_alloc::internal

// TODO(crbug.com/1288247): Remove these when migration is complete.
namespace base::internal {

using ::partition_alloc::internal::Context;
using ::partition_alloc::internal::SimdSupport;

}  // namespace base::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STARSCAN_FWD_H_
