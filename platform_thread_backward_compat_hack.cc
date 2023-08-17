// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

// HACK: For some reason chromeos-base/ec-utils is built with an old version
// libchrome (probably improper dependency structure in ebuilds).
// Manually export this function to make ec-utils happy during transition.
// TODO(b/293252981): Remove this after a few days.
extern "C" BASE_EXPORT void _ZN4base14PlatformThread5SleepENS_9TimeDeltaE(base::TimeDelta duration) {
  base::PlatformThread::Sleep(duration);
}
