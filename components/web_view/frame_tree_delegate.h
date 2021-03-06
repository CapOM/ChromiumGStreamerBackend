// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FRAME_TREE_DELEGATE_H_
#define COMPONENTS_WEB_VIEW_FRAME_TREE_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/web_view/public/interfaces/frame_tree.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace web_view {

class Frame;
class FrameUserData;
class HTMLMessageEvent;

class FrameTreeDelegate {
 public:
  // Callback from CanNavigateFrame(). The uint32_t is the id of the app the
  // FrameTreeClient comes from; typically the content handler id.
  using CanNavigateFrameCallback =
      base::Callback<void(uint32_t,
                          FrameTreeClient*,
                          scoped_ptr<FrameUserData>,
                          mojo::ViewTreeClientPtr)>;

  // Returns whether a request to post a message from |source| to |target|
  // is allowed. |source| and |target| are never null.
  virtual bool CanPostMessageEventToFrame(const Frame* source,
                                          const Frame* target,
                                          HTMLMessageEvent* event) = 0;

  virtual void LoadingStateChanged(bool loading) = 0;
  virtual void ProgressChanged(double progress) = 0;

  virtual void TitleChanged(const mojo::String& title) = 0;

  // |source| is requesting a navigation. If |target_type| is
  // |EXISTING_FRAME| then |target_frame| identifies the frame to perform the
  // navigation in, otherwise |target_frame| is not used. |target_frame| may
  // be null, even for |EXISTING_FRAME|.
  // TODO(sky): this needs to distinguish between navigate in source, vs new
  // background tab, vs new foreground tab.
  virtual void NavigateTopLevel(Frame* source, mojo::URLRequestPtr request) = 0;

  // Asks the client if navigation is allowed. If the navigation is allowed
  // |callback| should be called to continue the navigation. |callback|
  // may be called synchronously or asynchronously. In the callback
  // mojo::ViewTreeClientPtr should only be set if an app other than
  // frame->app_id() is used to render |request|.
  virtual void CanNavigateFrame(Frame* target,
                                mojo::URLRequestPtr request,
                                const CanNavigateFrameCallback& callback) = 0;

  // Invoked when a navigation in |frame| has been initiated.
  virtual void DidStartNavigation(Frame* frame) = 0;

  // Notification of various frame state changes. Generally only useful for
  // tests.
  virtual void DidCreateFrame(Frame* frame);
  virtual void DidDestroyFrame(Frame* frame);

  // Invoked when the View embedded in a Frame premuturely stops rendering
  // to |frame|'s View. This could mean any of the following:
  // . The app crashed.
  // . There is no app that renders the url.
  // . The app is still alive, but is shutting down.
  // Frame does nothing in response to this, but the delegate may wish to take
  // action.
  virtual void OnViewEmbeddedInFrameDisconnected(Frame* frame);

 protected:
  virtual ~FrameTreeDelegate() {}
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FRAME_TREE_DELEGATE_H_
