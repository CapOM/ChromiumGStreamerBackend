// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_LAYER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_LAYER_IMPL_H_

#include <utility>

#include "cc/blink/web_layer_impl.h"
#include "cc/layers/surface_layer.h"

namespace mus {
class View;
}

namespace html_viewer {

class WebLayerImpl : public cc_blink::WebLayerImpl {
 public:
  WebLayerImpl(mus::View* view, float device_pixel_ratio);
  ~WebLayerImpl() override;

  // WebLayer implementation.
  void setBounds(const blink::WebSize& bounds) override;

 private:
  mus::View* view_;
  const float device_pixel_ratio_;
  scoped_refptr<cc::SurfaceLayer> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_LAYER_IMPL_H_
