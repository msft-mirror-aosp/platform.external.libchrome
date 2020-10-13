// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <pthread.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

// Barebones TLS implementation for use in PartitionAlloc. This doesn't use the
// general chromium TLS handling to avoid dependencies, but more importantly
// because it allocates memory.
namespace base {
namespace internal {

#if defined(OS_POSIX)
typedef pthread_key_t PartitionTlsKey;

ALWAYS_INLINE bool PartitionTlsCreate(PartitionTlsKey* key,
                                      void (*destructor)(void*)) {
  return !pthread_key_create(key, destructor);
}
ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
  return pthread_getspecific(key);
}
ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  int ret = pthread_setspecific(key, value);
  PA_DCHECK(!ret);
}
#elif defined(OS_WIN)
// Note: supports only a single TLS key on Windows. Not a hard constraint, may
// be lifted.
typedef unsigned long PartitionTlsKey;

BASE_EXPORT bool PartitionTlsCreate(PartitionTlsKey* key,
                                    void (*destructor)(void*));

ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
  return TlsGetValue(key);
}

ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  BOOL ret = TlsSetValue(key, value);
  PA_DCHECK(ret);
}
#else
// Not supported.
typedef int PartitionTlsKey;
ALWAYS_INLINE bool PartitionTlsCreate(PartitionTlsKey* key,
                                      void (*destructor)(void*)) {
  // NOTIMPLEMENTED() may allocate, crash instead.
  IMMEDIATE_CRASH();
}
ALWAYS_INLINE void* PartitionTlsGet(PartitionTlsKey key) {
  IMMEDIATE_CRASH();
}
ALWAYS_INLINE void PartitionTlsSet(PartitionTlsKey key, void* value) {
  IMMEDIATE_CRASH();
}
#endif  // defined(OS_WIN)

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TLS_H_
