// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_AVDA_RETURN_ON_FAILURE_H_
#define CONTENT_COMMON_GPU_MEDIA_AVDA_RETURN_ON_FAILURE_H_

#include "media/video/video_decode_accelerator.h"

// Helper macros for dealing with failure.  If |result| evaluates false, emit
// |log| to ERROR, register |error| with the decoder, and return.  This will
// also transition to the error state, stopping further decoding.
// This is meant to be used only within AndroidVideoDecoder and the various
// backing strategies.  |provider| must support PostError.
#define RETURN_ON_FAILURE(provider, result, log, error)                     \
  do {                                                                      \
    if (!(result)) {                                                        \
      DLOG(ERROR) << log;                                                   \
      provider->PostError(FROM_HERE, media::VideoDecodeAccelerator::error); \
      return;                                                               \
    }                                                                       \
  } while (0)

#endif  // CONTENT_COMMON_GPU_MEDIA_AVDA_RETURN_ON_FAILURE_H_
