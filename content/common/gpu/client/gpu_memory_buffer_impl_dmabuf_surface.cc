// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_dmabuf_surface.h"

#include <utility>

#include "ui/gfx/buffer_format_util.h"

namespace content {
namespace {

}  // namespace

GpuMemoryBufferImplDmabufSurface::GpuMemoryBufferImplDmabufSurface(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const DestructionCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle)
    : GpuMemoryBufferImpl(id, size, format, callback),
      handle_(handle) {}

GpuMemoryBufferImplDmabufSurface::~GpuMemoryBufferImplDmabufSurface() {}

// static
scoped_ptr<GpuMemoryBufferImplDmabufSurface>
GpuMemoryBufferImplDmabufSurface::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  return make_scoped_ptr(new GpuMemoryBufferImplDmabufSurface(
      handle.id, size, format, callback, handle));
}

// static
bool GpuMemoryBufferImplDmabufSurface::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  if (usage != gfx::BufferUsage::SCANOUT)
    return false;

  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return true;
    default:
      return false;
  }

  return false;
}

bool GpuMemoryBufferImplDmabufSurface::Map() {
  return false;
}

void* GpuMemoryBufferImplDmabufSurface::memory(size_t plane) {
  return nullptr;
}

void GpuMemoryBufferImplDmabufSurface::Unmap() {
}

int GpuMemoryBufferImplDmabufSurface::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  return handle_.stride;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplDmabufSurface::GetHandle() const {
  return handle_;
}

}  // namespace content
