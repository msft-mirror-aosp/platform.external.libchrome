#ifndef BUILD_OS_BUILDFLAGS_H_
#define BUILD_OS_BUILDFLAGS_H_

#include "build/buildflag.h"

#define BUILDFLAG_INTERNAL_IS_ANDROID() (0)
#define BUILDFLAG_INTERNAL_IS_CHROMEOS() (1)
#define BUILDFLAG_INTERNAL_IS_FUCHSIA() (0)
#define BUILDFLAG_INTERNAL_IS_IOS() (0)
#define BUILDFLAG_INTERNAL_IS_LINUX() (0)
#define BUILDFLAG_INTERNAL_IS_MAC() (0)
#define BUILDFLAG_INTERNAL_IS_NACL() (0)
#define BUILDFLAG_INTERNAL_IS_WIN() (0)
#define BUILDFLAG_INTERNAL_IS_APPLE() (0)
#define BUILDFLAG_INTERNAL_IS_POSIX() (1)

#endif  // BUILD_OS_BUILDFLAGS_H_