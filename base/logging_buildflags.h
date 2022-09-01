// This buildflag header file will remain manually edited since value of NDEBUG
// or DCHECK_ALWAYS_ON cannot be passed from BUILD.gn using buildflag_header.

#ifndef BASE_LOGGING_BUILDFLAGS_H_
#define BASE_LOGGING_BUILDFLAGS_H_
#include "build/buildflag.h"
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define BUILDFLAG_INTERNAL_ENABLE_LOG_ERROR_NOT_REACHED() (1)
#else
#define BUILDFLAG_INTERNAL_ENABLE_LOG_ERROR_NOT_REACHED() (0)
#endif
#define BUILDFLAG_INTERNAL_USE_RUNTIME_VLOG() (1)
#endif  // BASE_LOGGING_BUILDFLAGS_H_
