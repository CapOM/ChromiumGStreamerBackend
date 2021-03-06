// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "components/mus/public/cpp/view_observer.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "components/mus/public/cpp/view_tree_host_factory.h"
#include "components/web_view/frame.h"
#include "components/web_view/frame_connection.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/frame_user_data.h"
#include "components/web_view/test_frame_tree_delegate.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "mojo/application/public/cpp/service_provider_impl.h"

using mus::View;
using mus::ViewTreeConnection;

namespace web_view {

namespace {

base::RunLoop* current_run_loop = nullptr;

void TimeoutRunLoop(const base::Closure& timeout_task, bool* timeout) {
  CHECK(current_run_loop);
  *timeout = true;
  timeout_task.Run();
}

bool DoRunLoopWithTimeout() {
  if (current_run_loop != nullptr)
    return false;

  bool timeout = false;
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&TimeoutRunLoop, run_loop.QuitClosure(), &timeout),
      TestTimeouts::action_timeout());

  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = nullptr;
  return !timeout;
}

void QuitRunLoop() {
  current_run_loop->Quit();
  current_run_loop = nullptr;
}

}  // namespace

void OnGotIdCallback(base::RunLoop* run_loop) {
  run_loop->Quit();
}

// Creates a new FrameConnection. This runs a nested message loop until the
// content handler id is obtained.
scoped_ptr<FrameConnection> CreateFrameConnection(mojo::ApplicationImpl* app) {
  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(app->url());
  base::RunLoop run_loop;
  frame_connection->Init(app, request.Pass(),
                         base::Bind(&OnGotIdCallback, &run_loop));
  run_loop.Run();
  return frame_connection;
}

class TestFrameTreeClient : public FrameTreeClient {
 public:
  TestFrameTreeClient() : connect_count_(0) {}
  ~TestFrameTreeClient() override {}

  int connect_count() const { return connect_count_; }

  mojo::Array<FrameDataPtr> connect_frames() { return connect_frames_.Pass(); }

  mojo::Array<FrameDataPtr> adds() { return adds_.Pass(); }

  // Sets a callback to run once OnConnect() is received.
  void set_on_connect_callback(const base::Closure& closure) {
    on_connect_callback_ = closure;
  }

  FrameTreeServer* server() { return server_.get(); }

  // TestFrameTreeClient:
  void OnConnect(FrameTreeServerPtr server,
                 uint32_t change_id,
                 uint32_t view_id,
                 ViewConnectType view_connect_type,
                 mojo::Array<FrameDataPtr> frames,
                 const OnConnectCallback& callback) override {
    connect_count_++;
    connect_frames_ = frames.Pass();
    server_ = server.Pass();
    callback.Run();
    if (!on_connect_callback_.is_null())
      on_connect_callback_.Run();
  }
  void OnFrameAdded(uint32_t change_id, FrameDataPtr frame) override {
    adds_.push_back(frame.Pass());
  }
  void OnFrameRemoved(uint32_t change_id, uint32_t frame_id) override {}
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_data) override {}
  void OnPostMessageEvent(uint32_t source_frame_id,
                          uint32_t target_frame_id,
                          HTMLMessageEventPtr event) override {}
  void OnWillNavigate(uint32_t frame_id) override {}

 private:
  int connect_count_;
  mojo::Array<FrameDataPtr> connect_frames_;
  FrameTreeServerPtr server_;
  mojo::Array<FrameDataPtr> adds_;
  base::Closure on_connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameTreeClient);
};

class FrameTest;

// ViewAndFrame maintains the View and TestFrameTreeClient associated with
// a single FrameTreeClient. In other words this maintains the data structures
// needed to represent a client side frame. To obtain one use
// FrameTest::WaitForViewAndFrame().
class ViewAndFrame : public mus::ViewTreeDelegate {
 public:
  ~ViewAndFrame() override {
    if (view_)
      delete view_->connection();
  }

  // The View associated with the frame.
  mus::View* view() { return view_; }
  TestFrameTreeClient* test_frame_tree_client() {
    return &test_frame_tree_client_;
  }
  FrameTreeServer* frame_tree_server() {
    return test_frame_tree_client_.server();
  }

 private:
  friend class FrameTest;

  ViewAndFrame()
      : view_(nullptr), frame_tree_binding_(&test_frame_tree_client_) {}

  // Runs a message loop until the view and frame data have been received.
  void WaitForViewAndFrame() { run_loop_.Run(); }

  void Bind(mojo::InterfaceRequest<FrameTreeClient> request) {
    ASSERT_FALSE(frame_tree_binding_.is_bound());
    test_frame_tree_client_.set_on_connect_callback(
        base::Bind(&ViewAndFrame::OnGotConnect, base::Unretained(this)));
    frame_tree_binding_.Bind(request.Pass());
  }

  void OnGotConnect() { QuitRunLoopIfNecessary(); }

  void QuitRunLoopIfNecessary() {
    if (view_ && test_frame_tree_client_.connect_count())
      run_loop_.Quit();
  }

  // Overridden from ViewTreeDelegate:
  void OnEmbed(View* root) override {
    view_ = root;
    QuitRunLoopIfNecessary();
  }
  void OnConnectionLost(ViewTreeConnection* connection) override {
    view_ = nullptr;
  }

  mus::View* view_;
  base::RunLoop run_loop_;
  TestFrameTreeClient test_frame_tree_client_;
  mojo::Binding<FrameTreeClient> frame_tree_binding_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndFrame);
};

class FrameTest : public mojo::test::ApplicationTestBase,
                  public mojo::ApplicationDelegate,
                  public mus::ViewTreeDelegate,
                  public mojo::InterfaceFactory<mojo::ViewTreeClient>,
                  public mojo::InterfaceFactory<FrameTreeClient> {
 public:
  FrameTest() : most_recent_connection_(nullptr), window_manager_(nullptr) {}

  ViewTreeConnection* most_recent_connection() {
    return most_recent_connection_;
  }

 protected:
  ViewTreeConnection* window_manager() { return window_manager_; }
  TestFrameTreeDelegate* frame_tree_delegate() {
    return frame_tree_delegate_.get();
  }
  FrameTree* frame_tree() { return frame_tree_.get(); }
  ViewAndFrame* root_view_and_frame() { return root_view_and_frame_.get(); }

  mojo::Binding<FrameTreeServer>* frame_tree_server_binding(Frame* frame) {
    return frame->frame_tree_server_binding_.get();
  }

  // Creates a new shared frame as a child of |parent|.
  Frame* CreateSharedFrame(ViewAndFrame* parent) {
    mus::View* child_frame_view = parent->view()->connection()->CreateView();
    parent->view()->AddChild(child_frame_view);
    mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties;
    client_properties.mark_non_null();
    parent->frame_tree_server()->OnCreatedFrame(
        child_frame_view->parent()->id(), child_frame_view->id(),
        client_properties.Pass());
    Frame* frame = frame_tree_delegate()->WaitForCreateFrame();
    return HasFatalFailure() ? nullptr : frame;
  }

  scoped_ptr<ViewAndFrame> CreateChildViewAndFrame(ViewAndFrame* parent) {
    // All frames start out initially shared.
    Frame* child_frame = CreateSharedFrame(parent);
    if (!child_frame)
      return nullptr;

    // Navigate the child frame, which should trigger a new ViewAndFrame.
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(application_impl()->url());
    parent->frame_tree_server()->RequestNavigate(
        NAVIGATION_TARGET_TYPE_EXISTING_FRAME, child_frame->id(),
        request.Pass());
    return WaitForViewAndFrame();
  }

  // Runs a message loop until the data necessary to represent to a client side
  // frame has been obtained.
  scoped_ptr<ViewAndFrame> WaitForViewAndFrame() {
    DCHECK(!view_and_frame_);
    view_and_frame_.reset(new ViewAndFrame);
    view_and_frame_->WaitForViewAndFrame();
    return view_and_frame_.Pass();
  }

 private:
  // ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override { return this; }

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<mojo::ViewTreeClient>(this);
    connection->AddService<FrameTreeClient>(this);
    return true;
  }

  // Overridden from ViewTreeDelegate:
  void OnEmbed(View* root) override {
    most_recent_connection_ = root->connection();
    QuitRunLoop();
  }
  void OnConnectionLost(ViewTreeConnection* connection) override {}

  // Overridden from testing::Test:
  void SetUp() override {
    ApplicationTestBase::SetUp();

    mus::CreateSingleViewTreeHost(application_impl(), this, &host_);

    ASSERT_TRUE(DoRunLoopWithTimeout());
    std::swap(window_manager_, most_recent_connection_);

    // Creates a FrameTree, which creates a single frame. Wait for the
    // FrameTreeClient to be connected to.
    frame_tree_delegate_.reset(new TestFrameTreeDelegate(application_impl()));
    scoped_ptr<FrameConnection> frame_connection =
        CreateFrameConnection(application_impl());
    FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
    mojo::ViewTreeClientPtr view_tree_client =
        frame_connection->GetViewTreeClient();
    mus::View* frame_root_view = window_manager()->CreateView();
    window_manager()->GetRoot()->AddChild(frame_root_view);
    frame_tree_.reset(
        new FrameTree(0u, frame_root_view, view_tree_client.Pass(),
                      frame_tree_delegate_.get(), frame_tree_client,
                      frame_connection.Pass(), Frame::ClientPropertyMap()));
    root_view_and_frame_ = WaitForViewAndFrame();
  }

  // Overridden from testing::Test:
  void TearDown() override {
    root_view_and_frame_.reset();
    frame_tree_.reset();
    frame_tree_delegate_.reset();
    ApplicationTestBase::TearDown();
  }

  // Overridden from mojo::InterfaceFactory<mojo::ViewTreeClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::ViewTreeClient> request) override {
    if (view_and_frame_) {
      mus::ViewTreeConnection::Create(view_and_frame_.get(), request.Pass());
    } else {
      mus::ViewTreeConnection::Create(this, request.Pass());
    }
  }

  // Overridden from mojo::InterfaceFactory<FrameTreeClient>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<FrameTreeClient> request) override {
    ASSERT_TRUE(view_and_frame_);
    view_and_frame_->Bind(request.Pass());
  }

  scoped_ptr<TestFrameTreeDelegate> frame_tree_delegate_;
  scoped_ptr<FrameTree> frame_tree_;
  scoped_ptr<ViewAndFrame> root_view_and_frame_;

  mojo::ViewTreeHostPtr host_;

  // Used to receive the most recent view manager loaded by an embed action.
  ViewTreeConnection* most_recent_connection_;
  // The View Manager connection held by the window manager (app running at the
  // root view).
  ViewTreeConnection* window_manager_;

  scoped_ptr<ViewAndFrame> view_and_frame_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(FrameTest);
};

// Verifies the FrameData supplied to the root FrameTreeClient::OnConnect().
TEST_F(FrameTest, RootFrameClientConnectData) {
  mojo::Array<FrameDataPtr> frames =
      root_view_and_frame()->test_frame_tree_client()->connect_frames();
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(root_view_and_frame()->view()->id(), frames[0]->frame_id);
  EXPECT_EQ(0u, frames[0]->parent_id);
}

// Verifies the FrameData supplied to a child FrameTreeClient::OnConnect().
TEST_F(FrameTest, ChildFrameClientConnectData) {
  scoped_ptr<ViewAndFrame> child_view_and_frame(
      CreateChildViewAndFrame(root_view_and_frame()));
  ASSERT_TRUE(child_view_and_frame);
  mojo::Array<FrameDataPtr> frames_in_child =
      child_view_and_frame->test_frame_tree_client()->connect_frames();
  // We expect 2 frames. One for the root, one for the child.
  ASSERT_EQ(2u, frames_in_child.size());
  EXPECT_EQ(frame_tree()->root()->id(), frames_in_child[0]->frame_id);
  EXPECT_EQ(0u, frames_in_child[0]->parent_id);
  EXPECT_EQ(child_view_and_frame->view()->id(), frames_in_child[1]->frame_id);
  EXPECT_EQ(frame_tree()->root()->id(), frames_in_child[1]->parent_id);
}

TEST_F(FrameTest, OnViewEmbeddedInFrameDisconnected) {
  scoped_ptr<ViewAndFrame> child_view_and_frame(
      CreateChildViewAndFrame(root_view_and_frame()));
  ASSERT_TRUE(child_view_and_frame);

  // Delete the ViewTreeConnection for the child, which should trigger
  // notification.
  delete child_view_and_frame->view()->connection();
  ASSERT_EQ(1u, frame_tree()->root()->children().size());
  ASSERT_NO_FATAL_FAILURE(frame_tree_delegate()->WaitForFrameDisconnected(
      frame_tree()->root()->children()[0]));
  ASSERT_EQ(1u, frame_tree()->root()->children().size());
}

TEST_F(FrameTest, CantSendProgressChangeTargettingWrongApp) {
  ASSERT_FALSE(frame_tree()->root()->IsLoading());

  scoped_ptr<ViewAndFrame> child_view_and_frame(
      CreateChildViewAndFrame(root_view_and_frame()));
  ASSERT_TRUE(child_view_and_frame);

  Frame* shared_frame = CreateSharedFrame(child_view_and_frame.get());

  // Send LoadingStarted() from the root targetting a frame from another
  // connection. It should be ignored (not update the loading status).
  root_view_and_frame()->frame_tree_server()->LoadingStarted(
      shared_frame->id());
  frame_tree_server_binding(frame_tree()->root())->WaitForIncomingMethodCall();
  EXPECT_FALSE(frame_tree()->root()->IsLoading());
}

}  // namespace web_view
