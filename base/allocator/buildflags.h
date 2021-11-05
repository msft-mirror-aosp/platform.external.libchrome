#ifndef BASE_ALLOCATOR_BUILDFLAGS_H_
#define BASE_ALLOCATOR_BUILDFLAGS_H_
#include "build/buildflag.h"
#define BUILDFLAG_INTERNAL_USE_ALLOCATOR_SHIM() (0)
#define BUILDFLAG_INTERNAL_USE_TCMALLOC() (0)
#define BUILDFLAG_INTERNAL_USE_PARTITION_ALLOC() (1)
#define BUILDFLAG_INTERNAL_USE_PARTITION_ALLOC_AS_MALLOC() (0)
#define BUILDFLAG_INTERNAL_USE_BACKUP_REF_PTR() (0)
#define BUILDFLAG_INTERNAL_PUT_REF_COUNT_IN_PREVIOUS_SLOT() (0)
#define BUILDFLAG_INTERNAL_USE_DEDICATED_PARTITION_FOR_ALIGNED_ALLOC() (0)
#define BUILDFLAG_INTERNAL_ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL() (0)
#define BUILDFLAG_INTERNAL_ENABLE_BACKUP_REF_PTR_SLOW_CHECKS() (0)
#define BUILDFLAG_INTERNAL_ENABLE_BRP_DIRECTMAP_SUPPORT() (0)
#define BUILDFLAG_INTERNAL_USE_BRP_POOL_BLOCKLIST() (0)
#endif  // BASE_ALLOCATOR_BUILDFLAGS_H_
