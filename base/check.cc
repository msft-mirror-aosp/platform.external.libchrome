// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"

#include "build/build_config.h"

// check.h is a widely included header and its size has significant impact on
// build time. Try not to raise this limit unless absolutely necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#ifndef NACL_TC_REV
#pragma clang max_tokens_here 17000
#endif

#include "base/check_op.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace logging {

CheckError CheckError::Check(const char* file,
                             int line,
                             const char* condition) {
  auto* const log_message = new LogMessage(file, line, LOGGING_FATAL);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::CheckOp(const char* file,
                               int line,
                               CheckOpResult* check_op_result) {
  auto* const log_message = new LogMessage(file, line, LOGGING_FATAL);
  log_message->stream() << "Check failed: " << check_op_result->message_;
  free(check_op_result->message_);
  check_op_result->message_ = nullptr;
  return CheckError(log_message);
}

CheckError CheckError::DCheck(const char* file,
                              int line,
                              const char* condition) {
  auto* const log_message = new LogMessage(file, line, LOGGING_DCHECK);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::DCheckOp(const char* file,
                                int line,
                                CheckOpResult* check_op_result) {
  auto* const log_message = new LogMessage(file, line, LOGGING_DCHECK);
  log_message->stream() << "Check failed: " << check_op_result->message_;
  free(check_op_result->message_);
  check_op_result->message_ = nullptr;
  return CheckError(log_message);
}

CheckError CheckError::PCheck(const char* file,
                              int line,
                              const char* condition) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if BUILDFLAG(IS_WIN)
  auto* const log_message =
      new Win32ErrorLogMessage(file, line, LOGGING_FATAL, err_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  auto* const log_message =
      new ErrnoLogMessage(file, line, LOGGING_FATAL, err_code);
#endif
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::PCheck(const char* file, int line) {
  return PCheck(file, line, "");
}

CheckError CheckError::DPCheck(const char* file,
                               int line,
                               const char* condition) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if BUILDFLAG(IS_WIN)
  auto* const log_message =
      new Win32ErrorLogMessage(file, line, LOGGING_DCHECK, err_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  auto* const log_message =
      new ErrnoLogMessage(file, line, LOGGING_DCHECK, err_code);
#endif
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::NotImplemented(const char* file,
                                      int line,
                                      const char* function) {
  auto* const log_message = new LogMessage(file, line, LOGGING_ERROR);
  log_message->stream() << "Not implemented reached in " << function;
  return CheckError(log_message);
}

std::ostream& CheckError::stream() {
  return log_message_->stream();
}

CheckError::~CheckError() {
  // Note: This function ends up in crash stack traces. If its full name
  // changes, the crash server's magic signature logic needs to be updated.
  // See cl/306632920.
  delete log_message_;
}

CheckError::CheckError(LogMessage* log_message) : log_message_(log_message) {}

void RawCheck(const char* message) {
  RawLog(LOGGING_FATAL, message);
}

void RawError(const char* message) {
  RawLog(LOGGING_ERROR, message);
}

}  // namespace logging
