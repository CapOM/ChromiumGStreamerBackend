// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/media/gstreamer/gpuprocess/client_egl.h"

#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gpu_memory_buffer.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace content {

static base::LazyInstance<CommandBufferProxyImpl*>
    g_thread_safe_cmd_impl = LAZY_INSTANCE_INITIALIZER;

bool ClientEGL_SetupCommandBufferProxy()
{
  gpu::gles2::GLES2Interface* gl_interface = ::gles2::GetGLContext();

  if (!gl_interface) {
    NOTREACHED() << "clientEglSetCommandBufferProxy, no gl interface";
    return false;
  }

  gpu::gles2::GLES2Implementation* gl_impl =
      reinterpret_cast<gpu::gles2::GLES2Implementation*>(gl_interface);

  if (!gl_impl) {
    NOTREACHED() << "clientEglSetCommandBufferProxy, no gles impl";
    return false;
  }

  gpu::GpuControl* gpu_ctrl = gl_impl->gpu_control();

  if (!gpu_ctrl) {
    NOTREACHED() << "ClientEGL_SetupCommandBufferProxy, no gpu control";
    return false;
  }

  if (gpu_ctrl->IsGpuChannelLost()) {
    NOTREACHED() << "ClientEGL_SetupCommandBufferProxy, gpu channel is lost";
    return false;
  }

  content::CommandBufferProxyImpl* cmd_impl =
      static_cast<content::CommandBufferProxyImpl*>(gpu_ctrl);

  if (!cmd_impl) {
    NOTREACHED() << "ClientEGL_SetupCommandBufferProxy, no cmd buffer proxy";
    return false;
  }

  g_thread_safe_cmd_impl.Get() = cmd_impl;

  return true;
}

#define FOURCC(a, b, c, d)                                        \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888 FOURCC('A', 'B', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888 FOURCC('X', 'B', '2', '4')

static gfx::BufferFormat drmFourCCToBufferFormat(EGLint fourcc) {
  switch (fourcc) {
    case DRM_FORMAT_ABGR8888:
      return gfx::BufferFormat::RGBA_8888;
    case DRM_FORMAT_XBGR8888:
      return gfx::BufferFormat::RGBX_8888;
    case DRM_FORMAT_ARGB8888:
      return gfx::BufferFormat::BGRA_8888;
    case DRM_FORMAT_XRGB8888:
      return gfx::BufferFormat::BGRX_8888;
    default:
      NOTREACHED();
      return gfx::BufferFormat::LAST;
  }

  NOTREACHED();
  return gfx::BufferFormat::LAST;
}

void GpuMemoryBufferDeleted(int empty, const gpu::SyncToken& sync_token) {
  // Nothing to do.
}

EGLImageKHR CreateEGLImageKHR(EGLDisplay dpy,
                              EGLContext ctx,
                              EGLenum target,
                              EGLClientBuffer buffer,
                              const EGLint* attrib_list) {
  if (!g_thread_safe_cmd_impl.Get()) {
    NOTREACHED() << "eglCreateImageKHR, no cmd buffer proxy";
    return EGL_NO_IMAGE_KHR;
  }

  if (ctx != EGL_NO_CONTEXT) {
    NOTREACHED() << "eglCreateImageKHR, ctx != EGL_NO_CONTEXT";
    return EGL_NO_IMAGE_KHR;
  }

  if (target != EGL_LINUX_DMA_BUF_EXT) {
    NOTREACHED() << "eglCreateImageKHR, target != EGL_LINUX_DMA_BUF_EXT";
    return EGL_NO_IMAGE_KHR;
  }

  if (buffer != static_cast<EGLClientBuffer>(nullptr)) {
    NOTREACHED() << "eglCreateImageKHR, Client buffer must be null";
    return EGL_NO_IMAGE_KHR;
  }

  if (!attrib_list) {
    NOTREACHED() << "eglCreateImageKHR, attrib_list is null";
    return EGL_NO_IMAGE_KHR;
  }

  EGLint width = 0;
  EGLint height = 0;
  EGLint nb_attribs = 0;
  EGLint fourcc = 0;
  EGLint offset = 0;
  EGLint stride = 0;
  base::ScopedFD fd;

  for (size_t i = 0; i < 20; ++i) {
    if (attrib_list[i] == EGL_WIDTH)
      width = attrib_list[i + 1];
    else if (attrib_list[i] == EGL_HEIGHT)
       height = attrib_list[i + 1];
    else if (attrib_list[i] == EGL_LINUX_DRM_FOURCC_EXT)
       fourcc = attrib_list[i + 1];
    else if (attrib_list[i] == EGL_DMA_BUF_PLANE0_OFFSET_EXT)
       offset = attrib_list[i + 1];
    else if (attrib_list[i] == EGL_DMA_BUF_PLANE0_PITCH_EXT)
       stride = attrib_list[i + 1];
    else if (attrib_list[i] == EGL_DMA_BUF_PLANE0_FD_EXT)
       fd.reset(attrib_list[i + 1]);
    else if (attrib_list[i] == EGL_NONE) {
      nb_attribs = i + 1;
      break;
    }
  }

  if (!nb_attribs) {
    NOTREACHED() << "eglCreateImageKHR, no attribs";
    return EGL_NO_IMAGE_KHR;
  }

  if (width <= 0) {
    NOTREACHED() << "eglCreateImageKHR, width <= 0";
    return EGL_NO_IMAGE_KHR;
  }

  if (height <= 0) {
    NOTREACHED() << "eglCreateImageKHR, height <= 0";
    return EGL_NO_IMAGE_KHR;
  }

  gfx::BufferFormat format(drmFourCCToBufferFormat(fourcc));

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::DMABUF_SURFACE_BUFFER;
  handle.handle = base::FileDescriptor(std::move(fd));
  handle.offset = offset;
  handle.stride = stride;

  scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      content::GpuMemoryBufferImpl::CreateFromHandle(handle, gfx::Size(width, height), format,
      gfx::BufferUsage::SCANOUT, base::Bind(&GpuMemoryBufferDeleted, 0));

  if (!gpu_memory_buffer) {
    NOTREACHED() << "failed to rcreate gpu_memory_buffer";
    return EGL_NO_IMAGE_KHR;
  }

  int32_t image_id = g_thread_safe_cmd_impl.Get()->CreateImage(gpu_memory_buffer->AsClientBuffer(), width, height, GL_RGBA);

  if (image_id < 0) {
    NOTREACHED() << "eglCreateImageKHR, image_id < 0";
    return EGL_NO_IMAGE_KHR;
  }

  return reinterpret_cast<EGLImageKHR>(image_id);
}

EGLBoolean DestroyEGLImageKHR(EGLDisplay dpy, EGLImageKHR img) {
  gpu::gles2::GLES2Interface* gl = ::gles2::GetGLContext();

  if (!gl) {
    NOTREACHED() << "eglDestroyEGLImageKHR, no gl interface";
    return EGL_FALSE;
  }

  if (img == EGL_NO_IMAGE_KHR) {
    NOTREACHED() << "eglDestroyEGLImageKHR, invalid egl image";
    return EGL_FALSE;
  }

  gl->DestroyImageCHROMIUM(reinterpret_cast<uintptr_t>(img));

  return EGL_TRUE;
}

}  // namespace content
