// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_SHARED_MEMORY_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_SHARED_MEMORY_H_

#include "base/memory/ref_counted.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if DCHECK_IS_ON()
#include <set>
#endif

namespace gfx {
class GLImage;
}

namespace content {

class GpuMemoryBufferFactorySharedMemory : public GpuMemoryBufferFactory,
                                           public gpu::ImageFactory {
 public:
  GpuMemoryBufferFactorySharedMemory();
  ~GpuMemoryBufferFactorySharedMemory() override;

  static bool IsGpuMemoryBufferConfigurationSupported(gfx::BufferFormat format,
                                                      gfx::BufferUsage usage);

  // Overridden from GpuMemoryBufferFactory:
  void GetSupportedGpuMemoryBufferConfigurations(
      std::vector<Configuration>* configurations) override;
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      gfx::PluginWindowHandle surface_handle) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id) override;
  gpu::ImageFactory* AsImageFactory() override;

  // Overridden from gpu::ImageFactory:
  scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      unsigned internalformat,
      int client_id) override;

 private:
#if DCHECK_IS_ON()
  std::set<gfx::GpuMemoryBufferId> buffers_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactorySharedMemory);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_SHARED_MEMORY_H_
