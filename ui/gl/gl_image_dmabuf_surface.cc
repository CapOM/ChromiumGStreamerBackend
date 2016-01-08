// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_dmabuf_surface.h"

// TODO make some drm format convertion helpers and also use them in
// gl_image_ozone_native_pixmap.cc.

#define FOURCC(a, b, c, d)                                        \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888 FOURCC('A', 'B', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888 FOURCC('X', 'B', '2', '4')

namespace gfx {
namespace {

bool ValidInternalFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_RGB:
    case GL_RGBA:
    case GL_BGRA_EXT:
      return true;
    default:
      return false;
  }
}

bool ValidFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::RGBA_8888:
    case BufferFormat::RGBX_8888:
    case BufferFormat::BGRA_8888:
    case BufferFormat::BGRX_8888:
      return true;
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::R_8:
    case BufferFormat::RGBA_4444:
    case BufferFormat::YUV_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::UYVY_422:
      return false;
  }

  NOTREACHED();
  return false;
}

EGLint FourCC(BufferFormat format) {
  switch (format) {
    case BufferFormat::RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case BufferFormat::BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::R_8:
    case BufferFormat::RGBA_4444:
    case BufferFormat::YUV_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

GLImageDmabufSurface::GLImageDmabufSurface(const Size& size,
                                           unsigned internalformat)
    : gl::GLImageEGL(size), internalformat_(internalformat) {}

GLImageDmabufSurface::~GLImageDmabufSurface() {
}

bool GLImageDmabufSurface::Initialize(const base::SharedMemoryHandle& handle,
                                          gfx::BufferFormat format,
                                          size_t offset,
                                          size_t stride) {
  if (!ValidInternalFormat(internalformat_)) {
    LOG(ERROR) << "Invalid internalformat: " << internalformat_;
    return false;
  }

  if (!ValidFormat(format)) {
    LOG(ERROR) << "Invalid format: " << static_cast<int>(format);
    return false;
  }

  // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
  // target, the EGL will take a reference to the dma_buf.
  EGLint attrs[] = {EGL_WIDTH,
                    size_.width(),
                    EGL_HEIGHT,
                    size_.height(),
                    EGL_LINUX_DRM_FOURCC_EXT,
                    FourCC(format),
                    EGL_DMA_BUF_PLANE0_FD_EXT,
                    handle.fd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                    offset,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT,
                    stride,
                    EGL_NONE};
  if (!gl::GLImageEGL::Initialize(EGL_LINUX_DMA_BUF_EXT,
                                  static_cast<EGLClientBuffer>(nullptr),
                                  attrs)) {
    return false;
  }

  return true;
}

unsigned GLImageDmabufSurface::GetInternalFormat() {
  return internalformat_;
}

void GLImageDmabufSurface::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {
  // TODO(ericrk): Implement GLImage OnMemoryDump. crbug.com/514914
}

}  // namespace gfx
