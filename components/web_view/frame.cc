// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_property.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/frame_user_data.h"
#include "components/web_view/frame_utils.h"
#include "mojo/application/public/interfaces/shell.mojom.h"

using mus::View;

DECLARE_VIEW_PROPERTY_TYPE(web_view::Frame*);

namespace web_view {

// Used to find the Frame associated with a View.
DEFINE_LOCAL_VIEW_PROPERTY_KEY(Frame*, kFrame, nullptr);

namespace {

const uint32_t kNoParentId = 0u;
const mus::ConnectionSpecificId kInvalidConnectionId = 0u;

FrameDataPtr FrameToFrameData(const Frame* frame) {
  FrameDataPtr frame_data(FrameData::New());
  frame_data->frame_id = frame->id();
  frame_data->parent_id = frame->parent() ? frame->parent()->id() : kNoParentId;
  frame_data->client_properties =
      mojo::Map<mojo::String, mojo::Array<uint8_t>>::From(
          frame->client_properties());
  return frame_data.Pass();
}

}  // namespace

struct Frame::FrameTreeServerBinding {
  scoped_ptr<FrameUserData> user_data;
  scoped_ptr<mojo::Binding<FrameTreeServer>> frame_tree_server_binding;
};

Frame::Frame(FrameTree* tree,
             View* view,
             uint32_t frame_id,
             uint32_t app_id,
             ViewOwnership view_ownership,
             FrameTreeClient* frame_tree_client,
             scoped_ptr<FrameUserData> user_data,
             const ClientPropertyMap& client_properties)
    : tree_(tree),
      view_(nullptr),
      embedded_connection_id_(kInvalidConnectionId),
      id_(frame_id),
      app_id_(app_id),
      parent_(nullptr),
      view_ownership_(view_ownership),
      user_data_(user_data.Pass()),
      frame_tree_client_(frame_tree_client),
      loading_(false),
      progress_(0.f),
      client_properties_(client_properties),
      embed_weak_ptr_factory_(this),
      navigate_weak_ptr_factory_(this) {
  if (view)
    SetView(view);
}

Frame::~Frame() {
  if (view_)
    view_->RemoveObserver(this);
  while (!children_.empty())
    delete children_[0];
  if (parent_)
    parent_->Remove(this);
  if (view_) {
    view_->ClearLocalProperty(kFrame);
    if (view_ownership_ == ViewOwnership::OWNS_VIEW)
      view_->Destroy();
  }
  tree_->delegate_->DidDestroyFrame(this);
}

void Frame::Init(Frame* parent, mojo::ViewTreeClientPtr view_tree_client) {
  {
    // Set the FrameTreeClient to null so that we don't notify the client of the
    // add before OnConnect().
    base::AutoReset<FrameTreeClient*> frame_tree_client_resetter(
        &frame_tree_client_, nullptr);
    if (parent)
      parent->Add(this);
  }

  InitClient(ClientType::NEW_APP, nullptr, view_tree_client.Pass());

  tree_->delegate_->DidCreateFrame(this);
}

// static
Frame* Frame::FindFirstFrameAncestor(View* view) {
  while (view && !view->GetLocalProperty(kFrame))
    view = view->parent();
  return view ? view->GetLocalProperty(kFrame) : nullptr;
}

const Frame* Frame::FindFrame(uint32_t id) const {
  if (id == id_)
    return this;

  for (const Frame* child : children_) {
    const Frame* match = child->FindFrame(id);
    if (match)
      return match;
  }
  return nullptr;
}

bool Frame::HasAncestor(const Frame* frame) const {
  const Frame* current = this;
  while (current && current != frame)
    current = current->parent_;
  return current == frame;
}

bool Frame::IsLoading() const {
  if (loading_)
    return true;
  for (const Frame* child : children_) {
    if (child->IsLoading())
      return true;
  }
  return false;
}

double Frame::GatherProgress(int* frame_count) const {
  ++(*frame_count);
  double progress = progress_;
  for (const Frame* child : children_)
    progress += child->GatherProgress(frame_count);
  return progress_;
}

void Frame::InitClient(
    ClientType client_type,
    scoped_ptr<FrameTreeServerBinding> frame_tree_server_binding,
    mojo::ViewTreeClientPtr view_tree_client) {
  if (client_type == ClientType::NEW_APP && view_tree_client.get()) {
    embedded_connection_id_ = kInvalidConnectionId;
    embed_weak_ptr_factory_.InvalidateWeakPtrs();
    view_->Embed(
        view_tree_client.Pass(), mojo::ViewTree::ACCESS_POLICY_DEFAULT,
        base::Bind(&Frame::OnEmbedAck, embed_weak_ptr_factory_.GetWeakPtr()));
  }

  std::vector<const Frame*> frames;
  tree_->root()->BuildFrameTree(&frames);

  mojo::Array<FrameDataPtr> array(frames.size());
  for (size_t i = 0; i < frames.size(); ++i)
    array[i] = FrameToFrameData(frames[i]).Pass();

  FrameTreeServerPtr frame_tree_server_ptr;
  // Don't install an error handler. We allow for the target to only implement
  // ViewTreeClient.
  frame_tree_server_binding_.reset(new mojo::Binding<FrameTreeServer>(
      this, GetProxy(&frame_tree_server_ptr).Pass()));
  if (frame_tree_client_) {
    frame_tree_client_->OnConnect(
        frame_tree_server_ptr.Pass(), tree_->change_id(), view_->id(),
        client_type == ClientType::SAME_APP ? VIEW_CONNECT_TYPE_USE_EXISTING
                                            : VIEW_CONNECT_TYPE_USE_NEW,
        array.Pass(),
        base::Bind(&OnConnectAck, base::Passed(&frame_tree_server_binding)));
    tree_->delegate_->DidStartNavigation(this);
  }
}

// static
void Frame::OnConnectAck(
    scoped_ptr<FrameTreeServerBinding> frame_tree_server_binding) {}

void Frame::ChangeClient(FrameTreeClient* frame_tree_client,
                         scoped_ptr<FrameUserData> user_data,
                         mojo::ViewTreeClientPtr view_tree_client,
                         uint32_t app_id) {
  while (!children_.empty())
    delete children_[0];

  ClientType client_type = view_tree_client.get() == nullptr
                               ? ClientType::SAME_APP
                               : ClientType::NEW_APP;
  scoped_ptr<FrameTreeServerBinding> frame_tree_server_binding;

  if (client_type == ClientType::SAME_APP) {
    // See comment in InitClient() for details.
    frame_tree_server_binding.reset(new FrameTreeServerBinding);
    frame_tree_server_binding->user_data = user_data_.Pass();
    frame_tree_server_binding->frame_tree_server_binding =
        frame_tree_server_binding_.Pass();
  }

  user_data_ = user_data.Pass();
  frame_tree_client_ = frame_tree_client;
  frame_tree_server_binding_.reset();
  loading_ = false;
  progress_ = 0.f;
  app_id_ = app_id;

  InitClient(client_type, frame_tree_server_binding.Pass(),
             view_tree_client.Pass());
}

void Frame::OnEmbedAck(bool success, mus::ConnectionSpecificId connection_id) {
  if (success)
    embedded_connection_id_ = connection_id;
}

void Frame::SetView(mus::View* view) {
  DCHECK(!view_);
  DCHECK_EQ(id_, view->id());
  view_ = view;
  view_->SetLocalProperty(kFrame, this);
  view_->AddObserver(this);
  if (pending_navigate_.get())
    StartNavigate(pending_navigate_.Pass());
}

Frame* Frame::GetAncestorWithFrameTreeClient() {
  Frame* frame = this;
  while (frame && !frame->frame_tree_client_)
    frame = frame->parent_;
  return frame;
}

void Frame::BuildFrameTree(std::vector<const Frame*>* frames) const {
  frames->push_back(this);
  for (const Frame* frame : children_)
    frame->BuildFrameTree(frames);
}

void Frame::Add(Frame* node) {
  DCHECK(!node->parent_);

  node->parent_ = this;
  children_.push_back(node);

  tree_->root()->NotifyAdded(this, node, tree_->AdvanceChangeID());
}

void Frame::Remove(Frame* node) {
  DCHECK_EQ(node->parent_, this);
  auto iter = std::find(children_.begin(), children_.end(), node);
  DCHECK(iter != children_.end());
  node->parent_ = nullptr;
  children_.erase(iter);

  tree_->root()->NotifyRemoved(this, node, tree_->AdvanceChangeID());
}

void Frame::StartNavigate(mojo::URLRequestPtr request) {
  pending_navigate_.reset();

  // We need a View to navigate. When we get the View we'll complete the
  // navigation.
  if (!view_) {
    pending_navigate_ = request.Pass();
    return;
  }

  // Drop any pending navigation requests.
  navigate_weak_ptr_factory_.InvalidateWeakPtrs();

  tree_->delegate_->CanNavigateFrame(
      this, request.Pass(),
      base::Bind(&Frame::OnCanNavigateFrame,
                 navigate_weak_ptr_factory_.GetWeakPtr()));
}

void Frame::OnCanNavigateFrame(uint32_t app_id,
                               FrameTreeClient* frame_tree_client,
                               scoped_ptr<FrameUserData> user_data,
                               mojo::ViewTreeClientPtr view_tree_client) {
  if (AreAppIdsEqual(app_id, app_id_)) {
    // The app currently rendering the frame will continue rendering it. In this
    // case we do not use the ViewTreeClient (because the app has a View already
    // and ends up reusing it).
    DCHECK(!view_tree_client.get());
  } else {
    Frame* ancestor_with_frame_tree_client = GetAncestorWithFrameTreeClient();
    DCHECK(ancestor_with_frame_tree_client);
    ancestor_with_frame_tree_client->frame_tree_client_->OnWillNavigate(id_);
    DCHECK(view_tree_client.get());
  }
  ChangeClient(frame_tree_client, user_data.Pass(), view_tree_client.Pass(),
               app_id);
}

void Frame::LoadingStartedImpl() {
  DCHECK(!loading_);
  loading_ = true;
  progress_ = 0.f;
  tree_->LoadingStateChanged();
}

void Frame::LoadingStoppedImpl() {
  DCHECK(loading_);
  loading_ = false;
  tree_->LoadingStateChanged();
}

void Frame::ProgressChangedImpl(double progress) {
  DCHECK(loading_);
  progress_ = progress;
  tree_->ProgressChanged();
}

void Frame::TitleChangedImpl(const mojo::String& title) {
  // Only care about title changes on the root frame.
  if (!parent_)
    tree_->TitleChanged(title);
}

void Frame::SetClientPropertyImpl(const mojo::String& name,
                                  mojo::Array<uint8_t> value) {
  auto iter = client_properties_.find(name);
  const bool already_in_map = (iter != client_properties_.end());
  if (value.is_null()) {
    if (!already_in_map)
      return;
    client_properties_.erase(iter);
  } else {
    std::vector<uint8_t> as_vector(value.To<std::vector<uint8_t>>());
    if (already_in_map && iter->second == as_vector)
      return;
    client_properties_[name] = as_vector;
  }
  tree_->ClientPropertyChanged(this, name, value);
}

Frame* Frame::FindFrameWithIdFromSameApp(uint32_t frame_id) {
  if (frame_id == id_)
    return this;  // Common case.

  Frame* frame = FindFrame(frame_id);
  if (!frame)
    return nullptr;

  for (Frame* test_frame = frame; test_frame != this;
       test_frame = test_frame->parent()) {
    if (test_frame->frame_tree_client_) {
      // The frame has it's own client/server pair. It should make requests
      // using
      // the server it has rather than an ancestor.
      DVLOG(1) << "ignoring request for a frame that has its own client.";
      return nullptr;
    }
  }

  return frame;
}

void Frame::NotifyAdded(const Frame* source,
                        const Frame* added_node,
                        uint32_t change_id) {
  if (frame_tree_client_)
    frame_tree_client_->OnFrameAdded(change_id, FrameToFrameData(added_node));

  for (Frame* child : children_)
    child->NotifyAdded(source, added_node, change_id);
}

void Frame::NotifyRemoved(const Frame* source,
                          const Frame* removed_node,
                          uint32_t change_id) {
  if (frame_tree_client_)
    frame_tree_client_->OnFrameRemoved(change_id, removed_node->id());

  for (Frame* child : children_)
    child->NotifyRemoved(source, removed_node, change_id);
}

void Frame::NotifyClientPropertyChanged(const Frame* source,
                                        const mojo::String& name,
                                        const mojo::Array<uint8_t>& value) {
  if (this != source && frame_tree_client_)
    frame_tree_client_->OnFrameClientPropertyChanged(source->id(), name,
                                                     value.Clone());

  for (Frame* child : children_)
    child->NotifyClientPropertyChanged(source, name, value);
}

void Frame::OnTreeChanged(const TreeChangeParams& params) {
  if (params.new_parent && this == tree_->root()) {
    Frame* child_frame = FindFrame(params.target->id());
    if (child_frame && !child_frame->view_)
      child_frame->SetView(params.target);
  }
}

void Frame::OnViewDestroying(mus::View* view) {
  if (parent_)
    parent_->Remove(this);

  // Reset |view_ownership_| so we don't attempt to delete |view_| in the
  // destructor.
  view_ownership_ = ViewOwnership::DOESNT_OWN_VIEW;

  if (tree_->root() == this) {
    view_->RemoveObserver(this);
    view_ = nullptr;
    return;
  }

  delete this;
}

void Frame::OnViewEmbeddedAppDisconnected(mus::View* view) {
  // See FrameTreeDelegate::OnViewEmbeddedAppDisconnected() for details of when
  // this happens.
  //
  // Currently we have no way to distinguish between the cases that lead to this
  // being called, so we assume we can continue on. Continuing on is important
  // for html as it's entirely possible for a page to create a frame, navigate
  // to a bogus url and expect the frame to still exist.
  //
  // TODO(sky): notify the delegate on this? At a minimum the delegate cares
  // if the root is unembedded as this would correspond to a sab tab.
  tree_->delegate_->OnViewEmbeddedInFrameDisconnected(this);
}

void Frame::PostMessageEventToFrame(uint32_t source_frame_id,
                                    uint32_t target_frame_id,
                                    HTMLMessageEventPtr event) {
  Frame* source = FindFrameWithIdFromSameApp(source_frame_id);
  // NOTE: |target_frame_id| is allowed to be from another connection.
  Frame* target = tree_->root()->FindFrame(target_frame_id);
  if (!target || !source || source == target || !tree_->delegate_ ||
      !tree_->delegate_->CanPostMessageEventToFrame(source, target,
                                                    event.get()))
    return;

  DCHECK(target->GetAncestorWithFrameTreeClient());
  target->GetAncestorWithFrameTreeClient()
      ->frame_tree_client_->OnPostMessageEvent(source_frame_id, target_frame_id,
                                               event.Pass());
}

void Frame::LoadingStarted(uint32_t frame_id) {
  Frame* target_frame = FindFrameWithIdFromSameApp(frame_id);
  if (target_frame)
    target_frame->LoadingStartedImpl();
}

void Frame::LoadingStopped(uint32_t frame_id) {
  Frame* target_frame = FindFrameWithIdFromSameApp(frame_id);
  if (target_frame)
    target_frame->LoadingStoppedImpl();
}

void Frame::ProgressChanged(uint32_t frame_id, double progress) {
  Frame* target_frame = FindFrameWithIdFromSameApp(frame_id);
  if (target_frame)
    target_frame->ProgressChangedImpl(progress);
}

void Frame::TitleChanged(uint32_t frame_id, const mojo::String& title) {
  Frame* target_frame = FindFrameWithIdFromSameApp(frame_id);
  if (target_frame)
    target_frame->TitleChangedImpl(title);
}

void Frame::SetClientProperty(uint32_t frame_id,
                              const mojo::String& name,
                              mojo::Array<uint8_t> value) {
  Frame* target_frame = FindFrameWithIdFromSameApp(frame_id);
  if (target_frame)
    target_frame->SetClientPropertyImpl(name, value.Pass());
}

void Frame::OnCreatedFrame(
    uint32_t parent_id,
    uint32_t frame_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties) {
  if ((frame_id >> 16) != embedded_connection_id_) {
    // TODO(sky): kill connection here?
    // TODO(sky): there is a race in that there is no guarantee we received the
    // connection id before the frame tries to create a new frame. Ideally we
    // could pause the frame until we get the connection id, but bindings don't
    // offer such an API.
    DVLOG(1) << "OnCreatedFrame supplied invalid frame id, expecting"
             << embedded_connection_id_;
    return;
  }

  if (FindFrame(frame_id)) {
    // TODO(sky): kill connection here?
    DVLOG(1) << "OnCreatedFrame supplied id of existing frame.";
    return;
  }

  Frame* parent_frame = FindFrameWithIdFromSameApp(parent_id);
  if (!parent_frame) {
    DVLOG(1) << "OnCreatedFrame supplied invalid parent_id.";
    return;
  }

  Frame* child_frame =
      tree_->CreateSharedFrame(parent_frame, frame_id, app_id_,
                               client_properties.To<ClientPropertyMap>());
  child_frame->embedded_connection_id_ = embedded_connection_id_;
}

void Frame::RequestNavigate(NavigationTargetType target_type,
                            uint32_t target_frame_id,
                            mojo::URLRequestPtr request) {
  if (target_type == NAVIGATION_TARGET_TYPE_EXISTING_FRAME) {
    // |target_frame| is allowed to come from another connection.
    Frame* target_frame = tree_->root()->FindFrame(target_frame_id);
    if (!target_frame) {
      DVLOG(1) << "RequestNavigate EXISTING_FRAME with no matching frame";
      return;
    }
    if (target_frame != tree_->root()) {
      target_frame->StartNavigate(request.Pass());
      return;
    }
    // Else case if |target_frame| == root. Treat at top level request.
  }
  tree_->delegate_->NavigateTopLevel(this, request.Pass());
}

void Frame::DidNavigateLocally(uint32_t frame_id, const mojo::String& url) {
  NOTIMPLEMENTED();
}

}  // namespace web_view
