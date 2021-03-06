// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_GLES2_OES_VERTEX_ARRAY_OBJECT_H_
#define MOJO_PUBLIC_C_GLES2_OES_VERTEX_ARRAY_OBJECT_H_

// Note: This header should be compilable as C.

#include <stdint.h>
#include <GLES2/gl2.h>

#include "mojo/public/c/gles2/gles2_export.h"
#include "mojo/public/c/gles2/gles2_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  MOJO_GLES2_EXPORT ReturnType GL_APIENTRY gl##Function PARAMETERS;
#include "mojo/public/c/gles2/gles2_call_visitor_oes_vertex_array_object_autogen.h"
#undef VISIT_GL_CALL

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_GLES2_OES_VERTEX_ARRAY_OBJECT_H_
