// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DUMP_WITHOUT_CRASHING_H_
#define DUMP_WITHOUT_CRASHING_H_

namespace libchrome_internal {

// Creates a crash dump without crashing the current process.
void DumpWithoutCrashing();

}  // namespace libchrome_internal

#endif  // DUMP_WITHOUT_CRASHING_H_
