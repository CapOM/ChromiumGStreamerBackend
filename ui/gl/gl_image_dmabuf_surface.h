// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_DMABUF_SURFACE_H_
#define UI_GL_GL_IMAGE_DMABUF_SURFACE_H_

#include <stdint.h>

#include "base/memory/shared_memory_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_image_egl.h"

namespace gfx {

class GL_EXPORT GLImageDmabufSurface : public gl::GLImageEGL {
 public:
  GLImageDmabufSurface(const Size& size, unsigned internalformat);

  bool Initialize(const base::SharedMemoryHandle& handle,
                  gfx::BufferFormat format,
                  size_t offset,
                  size_t stride);

  unsigned GetInternalFormat() override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 protected:
  ~GLImageDmabufSurface() override;

 private:
  unsigned internalformat_;
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_DMABUF_SURFACE_H_
