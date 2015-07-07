// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_

#include "base/id_map.h"
#include "components/guest_view/browser/guest_view.h"
#include "extensions/browser/guest_view/app_view/app_view_guest_delegate.h"

namespace extensions {
class Extension;
class ExtensionHost;

// An AppViewGuest provides the browser-side implementation of <appview> API.
// AppViewGuest is created on attachment. That is, when a guest WebContents is
// associated with a particular embedder WebContents. This happens on calls to
// the connect API.
class AppViewGuest : public guest_view::GuestView<AppViewGuest> {
 public:
  static const char Type[];

  // Completes the creation of a WebContents associated with the provided
  // |guest_extension_id| and |guest_instance_id| for the given
  // |browser_context|.
  // |guest_render_process_host| is the RenderProcessHost and |url| is the
  // resource GURL of the extension instance making this request. If there is
  // any mismatch between the expected |guest_instance_id| and
  // |guest_extension_id| provided and the recorded copies from when the the
  // <appview> was created, the RenderProcessHost of the extension instance
  // behind this request will be killed.
  static bool CompletePendingRequest(
      content::BrowserContext* browser_context,
      const GURL& url,
      int guest_instance_id,
      const std::string& guest_extension_id,
      content::RenderProcessHost* guest_render_process_host);

  static GuestViewBase* Create(content::WebContents* owner_web_contents);

  static std::vector<int> GetAllRegisteredInstanceIdsForTesting();

  // content::WebContentsDelegate implementation.
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override;
  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;

  // GuestViewBase implementation.
  bool CanRunInDetachedState() const override;
  void CreateWebContents(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) override;
  void DidInitialize(const base::DictionaryValue& create_params) override;
  const char* GetAPINamespace() const override;
  int GetTaskPrefix() const override;

  // Sets the AppDelegate for this guest.
  void SetAppDelegateForTest(AppDelegate* delegate);

 private:
  explicit AppViewGuest(content::WebContents* owner_web_contents);

  ~AppViewGuest() override;

  void CompleteCreateWebContents(const GURL& url,
                                 const Extension* guest_extension,
                                 const WebContentsCreatedCallback& callback);

  void LaunchAppAndFireEvent(scoped_ptr<base::DictionaryValue> data,
                             const WebContentsCreatedCallback& callback,
                             ExtensionHost* extension_host);

  GURL url_;
  std::string guest_extension_id_;
  scoped_ptr<AppViewGuestDelegate> app_view_guest_delegate_;
  scoped_ptr<AppDelegate> app_delegate_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<AppViewGuest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_
