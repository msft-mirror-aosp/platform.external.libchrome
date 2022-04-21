// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/random.h"

#include <cstddef>
#include <cstdint>

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include <limits>
#elif BUILDFLAG(IS_FUCHSIA)
#include <zircon/syscalls.h>
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <asm/unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif BUILDFLAG(IS_MAC)
#include <fcntl.h>
#include <sys/random.h>
#include <unistd.h>
#elif BUILDFLAG(IS_NACL)
#include <nacl/nacl_random.h>
#endif

#if BUILDFLAG(IS_WIN)
// #define needed to properly link in RtlGenRandom(), a.k.a. SystemFunction036.
// See the documentation here:
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036
#endif

namespace ipcz::reference_drivers {

void RandomBytes(absl::Span<uint8_t> destination) {
#if BUILDFLAG(IS_WIN)
  ABSL_ASSERT(destination.size() <= std::numeric_limits<ULONG>::max());
  const bool ok =
      RtlGenRandom(destination.data(), static_cast<ULONG>(destination.size()));
  ABSL_ASSERT(ok);
#elif BUILDFLAG(IS_FUCHSIA)
  zx_cprng_draw(destination.data(), destination.size());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  while (!destination.empty()) {
    ssize_t result =
        syscall(__NR_getrandom, destination.data(), destination.size(), 0);
    if (result == -1) {
      ABSL_ASSERT(errno == EINTR);
      continue;
    }

    destination.remove_prefix(result);
  }
#elif BUILDFLAG(IS_MAC)
  if (__builtin_available(macOS 10.12, *)) {
    const bool ok = getentropy(destination.data(), destination.size()) == 0;
    ABSL_ASSERT(ok);
  } else {
    // For macOS older than 10.12, fall back onto /dev/urandom, which we open
    // lazily on first call and leave open indefinitely.
    static int urandom_fd = [] {
      for (;;) {
        int result = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
        if (result >= 0) {
          return result;
        }
        ABSL_ASSERT(errno == EINTR);
      }
    }();

    while (!destination.empty()) {
      ssize_t result = read(urandom_fd, destination.data(), destination.size());
      if (result < 0) {
        ABSL_ASSERT(errno == EINTR);
        continue;
      }
      destination.remove_prefix(result);
    }
  }
#elif BUILDFLAG(IS_NACL)
  while (!destination.empty()) {
    size_t nread;
    nacl_secure_random(destination.data(), destination.size(), &nread);
    destination.remove_prefix(nread);
  }
#else
#error "Unsupported platform"
#endif
}

}  // namespace ipcz::reference_drivers
