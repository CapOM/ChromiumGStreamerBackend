// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_factory.h"

#include "base/logging.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

// static
RenderViewHostFactory* RenderViewHostFactory::factory_ = NULL;

// static
RenderViewHost* RenderViewHostFactory::Create(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int32 routing_id,
    int32 main_frame_routing_id,
    bool swapped_out,
    bool hidden) {
  int32 surface_id;
  // RenderViewHost creation can be either browser-driven (by the user opening a
  // new tab) or renderer-driven (by script calling window.open, etc).
  //
  // In the browser-driven case, the routing ID of the view is lazily assigned:
  // this is signified by passing MSG_ROUTING_NONE for |routing_id|.
  if (routing_id == MSG_ROUTING_NONE) {
    routing_id = instance->GetProcess()->GetNextRoutingID();
    // No surface has yet been created for the RVH.
    surface_id = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
        instance->GetProcess()->GetID(), routing_id);
  } else {
    // Otherwise, in the renderer-driven case, the routing ID of the view is
    // already set. This is due to the fact that a sync render->browser IPC is
    // involved. In order to quickly reply to the sync IPC, the routing IDs are
    // assigned as early as possible. The IO thread immediately sends a reply to
    // the sync IPC, while deferring the creation of the actual Host objects to
    // the UI thread. In this case, a surface already exists for the RVH.
    surface_id = GpuSurfaceTracker::Get()->LookupSurfaceForRenderer(
        instance->GetProcess()->GetID(), routing_id);
  }
  if (factory_) {
    return factory_->CreateRenderViewHost(instance, delegate, widget_delegate,
                                          routing_id, surface_id,
                                          main_frame_routing_id, swapped_out);
  }
  return new RenderViewHostImpl(instance, delegate, widget_delegate, routing_id,
                                surface_id, main_frame_routing_id, swapped_out,
                                hidden, true /* has_initialized_audio_host */);
}

// static
void RenderViewHostFactory::RegisterFactory(RenderViewHostFactory* factory) {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = factory;
}

// static
void RenderViewHostFactory::UnregisterFactory() {
  DCHECK(factory_) << "No factory to unregister.";
  factory_ = NULL;
}

}  // namespace content
