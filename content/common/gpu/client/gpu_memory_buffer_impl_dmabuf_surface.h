// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_DMABUF_SURFACE_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_DMABUF_SURFACE_H_

#include <stddef.h>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace content {

// Implementation of GPU memory buffer based on dmabuf.
class CONTENT_EXPORT GpuMemoryBufferImplDmabufSurface
    : public GpuMemoryBufferImpl {
 public:
  ~GpuMemoryBufferImplDmabufSurface() override;

  static scoped_ptr<GpuMemoryBufferImplDmabufSurface> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const DestructionCallback& callback);

  static bool IsConfigurationSupported(gfx::BufferFormat format,
                                       gfx::BufferUsage usage);

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

 private:
  GpuMemoryBufferImplDmabufSurface(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      const DestructionCallback& callback,
      const gfx::GpuMemoryBufferHandle& handle);

  gfx::GpuMemoryBufferHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplDmabufSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_DMABUF_SURFACE_H_
