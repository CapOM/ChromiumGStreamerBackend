// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_DISPATCHER_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_DISPATCHER_H_

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_process_observer.h"
#include "ipc/ipc_message.h"

namespace guest_view {

// Dispatcher used to route messages to GuestViewContainer.
class GuestViewContainerDispatcher : public content::RenderProcessObserver {
 public:
  GuestViewContainerDispatcher();
  ~GuestViewContainerDispatcher() override;

 protected:
  // Returns true if |message| is handled for a GuestViewContainer.
  virtual bool HandlesMessage(const IPC::Message& message);

 private:
  // content::RenderProcessObserver implementation.
  bool OnControlMessageReceived(const IPC::Message& message) override;

  DISALLOW_COPY_AND_ASSIGN(GuestViewContainerDispatcher);
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_DISPATCHER_H_
