// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dump_without_crashing.h>

#include <sys/wait.h>
#include <unistd.h>

#include <base/check.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

namespace libchrome_internal {

void DumpWithoutCrashing() {
    // Create a child process and crash it immediately.
    pid_t pid = fork();
    if (pid == 0) {
        logging::RawCheckFailure(
            "Crashing the child process for DumpWithoutCrashing().");
    }
    if (pid == -1) {
        PLOG(ERROR) << "fork() failed";
        return;
    }
    // Wait for the child process.
    auto ret = HANDLE_EINTR(waitpid(pid, nullptr, 0));
    if (ret == -1) {
      PLOG(ERROR) << "waitpid() failed for pid = " << pid;
    }
}

}  // namespace libchrome_internal
