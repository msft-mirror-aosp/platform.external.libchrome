// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_SHARED_BUFFER_MAPPING_H_
#define MOJO_CORE_IPCZ_DRIVER_SHARED_BUFFER_MAPPING_H_

#include <cstdint>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo::core::ipcz_driver {

// An active memory mapping of a driver-controlled shared buffer. Note that this
// is only used to manage read/writable mappings of unsafe regions by ipcz
// internals.
class SharedBufferMapping : public Object<SharedBufferMapping> {
 public:
  SharedBufferMapping(std::unique_ptr<base::SharedMemoryMapping> mapping,
                      void* memory);

  static constexpr Type object_type() { return kSharedBufferMapping; }

  void* memory() const { return memory_; }

  static scoped_refptr<SharedBufferMapping> Create(
      base::subtle::PlatformSharedMemoryRegion& region,
      size_t offset,
      size_t size);

  // Maps the whole region.
  static scoped_refptr<SharedBufferMapping> Create(
      base::subtle::PlatformSharedMemoryRegion& region);

  // Object:
  void Close() override;

 private:
  ~SharedBufferMapping() override;

  std::unique_ptr<base::SharedMemoryMapping> mapping_;
  void* memory_;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_SHARED_BUFFER_MAPPING_H_
