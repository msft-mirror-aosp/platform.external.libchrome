// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_YIELD_PROCESSOR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_YIELD_PROCESSOR_H_

#include "build/build_config.h"

// The PA_YIELD_PROCESSOR macro wraps an architecture specific-instruction that
// informs the processor we're in a busy wait, so it can handle the branch more
// intelligently and e.g. reduce power to our core or give more resources to the
// other hyper-thread on this core. See the following for context:
// https://software.intel.com/en-us/articles/benefitting-power-and-performance-sleep-loops

#if BUILDFLAG(IS_NACL)
// Inline assembly not allowed.
#define PA_YIELD_PROCESSOR ((void)0)
#elif defined(COMPILER_MSVC) && !defined(__clang__)
// MSVC is in its own assemblyless world (crbug.com/1351310#c6).
#include <windows.h>
#define PA_YIELD_PROCESSOR (YieldProcessor())
#else

#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_X86)
#define PA_YIELD_PROCESSOR __asm__ __volatile__("pause")
#elif (defined(ARCH_CPU_ARMEL) && __ARM_ARCH >= 6) || defined(ARCH_CPU_ARM64)
#define PA_YIELD_PROCESSOR __asm__ __volatile__("yield")
#elif defined(ARCH_CPU_MIPSEL)
// The MIPS32 docs state that the PAUSE instruction is a no-op on older
// architectures (first added in MIPS32r2). To avoid assembler errors when
// targeting pre-r2, we must encode the instruction manually.
#define PA_YIELD_PROCESSOR __asm__ __volatile__(".word 0x00000140")
#elif defined(ARCH_CPU_MIPS64EL) && __mips_isa_rev >= 2
// Don't bother doing using .word here since r2 is the lowest supported mips64
// that Chromium supports.
#define PA_YIELD_PROCESSOR __asm__ __volatile__("pause")
#elif defined(ARCH_CPU_PPC64_FAMILY)
#define PA_YIELD_PROCESSOR __asm__ __volatile__("or 31,31,31")
#elif defined(ARCH_CPU_S390_FAMILY)
// just do nothing
#define PA_YIELD_PROCESSOR ((void)0)
#endif  // ARCH

#ifndef PA_YIELD_PROCESSOR
#define PA_YIELD_PROCESSOR ((void)0)
#endif

#endif  // BUILDFLAG(IS_NACL)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_YIELD_PROCESSOR_H_
