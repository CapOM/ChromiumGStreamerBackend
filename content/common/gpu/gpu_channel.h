// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_result_codes.h"
#include "content/common/gpu/gpu_stream_priority.h"
#include "content/common/message_router.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_preference.h"

struct GPUCreateCommandBufferConfig;

namespace base {
class WaitableEvent;
}

namespace gpu {
class PreemptionFlag;
class SyncPointManager;
union ValueState;
class ValueStateMap;
namespace gles2 {
class SubscriptionRefSet;
}
}

namespace IPC {
class MessageFilter;
}

namespace content {
class GpuChannelManager;
class GpuChannelMessageFilter;
class GpuChannelMessageQueue;
class GpuJpegDecodeAccelerator;
class GpuWatchdog;

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class CONTENT_EXPORT GpuChannel
    : public IPC::Listener,
      public IPC::Sender,
      public gpu::gles2::SubscriptionRefSet::Observer {
 public:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuChannelManager* gpu_channel_manager,
             GpuWatchdog* watchdog,
             gfx::GLShareGroup* share_group,
             gpu::gles2::MailboxManager* mailbox_manager,
             base::SingleThreadTaskRunner* task_runner,
             base::SingleThreadTaskRunner* io_task_runner,
             int client_id,
             uint64_t client_tracing_id,
             bool software,
             bool allow_future_sync_points,
             bool allow_real_time_streams);
  ~GpuChannel() override;

  // Initializes the IPC channel. Caller takes ownership of the client FD in
  // the returned handle and is responsible for closing it.
  virtual IPC::ChannelHandle Init(base::WaitableEvent* shutdown_event);

  // Get the GpuChannelManager that owns this channel.
  GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_;
  }

  const std::string& channel_id() const { return channel_id_; }

  virtual base::ProcessId GetClientPID() const;

  int client_id() const { return client_id_; }

  uint64_t client_tracing_id() const { return client_tracing_id_; }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // Requeue the message that is currently being processed to the beginning of
  // the queue. Used when the processing of a message gets aborted because of
  // unscheduling conditions.
  void RequeueMessage();

  // SubscriptionRefSet::Observer implementation
  void OnAddSubscription(unsigned int target) override;
  void OnRemoveSubscription(unsigned int target) override;

  // This is called when a command buffer transitions from the unscheduled
  // state to the scheduled state, which potentially means the channel
  // transitions from the unscheduled to the scheduled state. When this occurs
  // deferred IPC messaged are handled.
  void OnScheduled();

  // This is called when a command buffer transitions between scheduled and
  // descheduled states. When any stub is descheduled, we stop preempting
  // other channels.
  void StubSchedulingChanged(bool scheduled);

  CreateCommandBufferResult CreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& window,
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id);

  gfx::GLShareGroup* share_group() const { return share_group_.get(); }

  GpuCommandBufferStub* LookupCommandBuffer(int32 route_id);

  void LoseAllContexts();
  void MarkAllContextsLost();

  // Called to add a listener for a particular message routing ID.
  // Returns true if succeeded.
  bool AddRoute(int32 route_id, IPC::Listener* listener);

  // Called to remove a listener for a particular message routing ID.
  void RemoveRoute(int32 route_id);

  gpu::PreemptionFlag* GetPreemptionFlag();

  // If |preemption_flag->IsSet()|, any stub on this channel
  // should stop issuing GL commands. Setting this to NULL stops deferral.
  void SetPreemptByFlag(
      scoped_refptr<gpu::PreemptionFlag> preemption_flag);

  void CacheShader(const std::string& key, const std::string& shader);

  void AddFilter(IPC::MessageFilter* filter);
  void RemoveFilter(IPC::MessageFilter* filter);

  uint64 GetMemoryUsage();

  scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      uint32 internalformat);

#if defined(USE_GSTREAMER)
  scoped_refptr<gfx::GLImage> CreateEGLImage(
      const gfx::Size& size,
      const std::vector<int32>& attributes,
      const std::vector<int32>& dmabuf_fds);
#endif

  bool allow_future_sync_points() const { return allow_future_sync_points_; }

  void HandleUpdateValueState(unsigned int target,
                              const gpu::ValueState& state);

  // Visible for testing.
  const gpu::ValueStateMap* pending_valuebuffer_state() const {
    return pending_valuebuffer_state_.get();
  }

  // Visible for testing.
  GpuChannelMessageFilter* filter() const { return filter_.get(); }

  uint32_t GetCurrentOrderNum() const { return current_order_num_; }
  uint32_t GetProcessedOrderNum() const { return processed_order_num_; }
  uint32_t GetUnprocessedOrderNum() const;

 protected:
  // The message filter on the io thread.
  scoped_refptr<GpuChannelMessageFilter> filter_;

  // Map of routing id to command buffer stub.
  base::ScopedPtrHashMap<int32, scoped_ptr<GpuCommandBufferStub>> stubs_;

 private:
  class StreamState {
   public:
    StreamState(int32 id, GpuStreamPriority priority);
    ~StreamState();

    int32 id() const { return id_; }
    GpuStreamPriority priority() const { return priority_; }

    void AddRoute(int32 route_id);
    void RemoveRoute(int32 route_id);
    bool HasRoute(int32 route_id) const;
    bool HasRoutes() const;

   private:
    int32 id_;
    GpuStreamPriority priority_;
    base::hash_set<int32> routes_;
  };

  friend class GpuChannelMessageFilter;
  friend class GpuChannelMessageQueue;

  void OnDestroy();

  bool OnControlMessageReceived(const IPC::Message& msg);

  void HandleMessage();

  // Message handlers.
  void OnCreateOffscreenCommandBuffer(
      const gfx::Size& size,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id,
      bool* succeeded);
  void OnDestroyCommandBuffer(int32 route_id);
  void OnCreateJpegDecoder(int32 route_id, IPC::Message* reply_msg);

  // Update processed order number and defer preemption.
  void MessageProcessed(uint32_t order_number);

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannelManager* gpu_channel_manager_;

  scoped_ptr<IPC::SyncChannel> channel_;

  // Uniquely identifies the channel within this GPU process.
  std::string channel_id_;

  // Used to implement message routing functionality to CommandBuffer objects
  MessageRouter router_;

  // Whether the processing of IPCs on this channel is stalled and we should
  // preempt other GpuChannels.
  scoped_refptr<gpu::PreemptionFlag> preempting_flag_;

  // If non-NULL, all stubs on this channel should stop processing GL
  // commands (via their GpuScheduler) when preempted_flag_->IsSet()
  scoped_refptr<gpu::PreemptionFlag> preempted_flag_;

  scoped_refptr<GpuChannelMessageQueue> message_queue_;

  // The id of the client who is on the other side of the channel.
  int client_id_;

  // The tracing ID used for memory allocations associated with this client.
  uint64_t client_tracing_id_;

  // The task runners for the main thread and the io thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The share group that all contexts associated with a particular renderer
  // process use.
  scoped_refptr<gfx::GLShareGroup> share_group_;

  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;

  scoped_refptr<gpu::gles2::SubscriptionRefSet> subscription_ref_set_;

  scoped_refptr<gpu::ValueStateMap> pending_valuebuffer_state_;

  scoped_ptr<GpuJpegDecodeAccelerator> jpeg_decoder_;

  gpu::gles2::DisallowedFeatures disallowed_features_;
  GpuWatchdog* watchdog_;
  bool software_;

  // Current IPC order number being processed.
  uint32_t current_order_num_;

  // Last finished IPC order number.
  uint32_t processed_order_num_;

  size_t num_stubs_descheduled_;

  // Map of stream id to stream state.
  base::hash_map<int32, StreamState> streams_;

  bool allow_future_sync_points_;
  bool allow_real_time_streams_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

// This filter does three things:
// - it counts and timestamps each message forwarded to the channel
//   so that we can preempt other channels if a message takes too long to
//   process. To guarantee fairness, we must wait a minimum amount of time
//   before preempting and we limit the amount of time that we can preempt in
//   one shot (see constants above).
// - it handles the GpuCommandBufferMsg_InsertSyncPoint message on the IO
//   thread, generating the sync point ID and responding immediately, and then
//   posting a task to insert the GpuCommandBufferMsg_RetireSyncPoint message
//   into the channel's queue.
// - it generates mailbox names for clients of the GPU process on the IO thread.
class GpuChannelMessageFilter : public IPC::MessageFilter {
 public:
  GpuChannelMessageFilter(
      scoped_refptr<GpuChannelMessageQueue> message_queue,
      gpu::SyncPointManager* sync_point_manager,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool future_sync_points);

  // IPC::MessageFilter implementation.
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelConnected(int32 peer_pid) override;
  void OnChannelError() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void AddChannelFilter(scoped_refptr<IPC::MessageFilter> filter);
  void RemoveChannelFilter(scoped_refptr<IPC::MessageFilter> filter);

  void OnMessageProcessed();

  void SetPreemptingFlagAndSchedulingState(gpu::PreemptionFlag* preempting_flag,
                                           bool a_stub_is_descheduled);

  void UpdateStubSchedulingState(bool a_stub_is_descheduled);

  bool Send(IPC::Message* message);

 protected:
  ~GpuChannelMessageFilter() override;

 private:
  enum PreemptionState {
    // Either there's no other channel to preempt, there are no messages
    // pending processing, or we just finished preempting and have to wait
    // before preempting again.
    IDLE,
    // We are waiting kPreemptWaitTimeMs before checking if we should preempt.
    WAITING,
    // We can preempt whenever any IPC processing takes more than
    // kPreemptWaitTimeMs.
    CHECKING,
    // We are currently preempting (i.e. no stub is descheduled).
    PREEMPTING,
    // We would like to preempt, but some stub is descheduled.
    WOULD_PREEMPT_DESCHEDULED,
  };

  void UpdatePreemptionState();

  void TransitionToIdleIfCaughtUp();
  void TransitionToIdle();
  void TransitionToWaiting();
  void TransitionToChecking();
  void TransitionToPreempting();
  void TransitionToWouldPreemptDescheduled();

  PreemptionState preemption_state_;

  // Maximum amount of time that we can spend in PREEMPTING.
  // It is reset when we transition to IDLE.
  base::TimeDelta max_preemption_time_;

  // The message_queue_ is used to handle messages on the main thread.
  scoped_refptr<GpuChannelMessageQueue> message_queue_;
  IPC::Sender* sender_;
  base::ProcessId peer_pid_;
  gpu::SyncPointManager* sync_point_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<gpu::PreemptionFlag> preempting_flag_;
  std::vector<scoped_refptr<IPC::MessageFilter>> channel_filters_;

  // This timer is created and destroyed on the IO thread.
  scoped_ptr<base::OneShotTimer<GpuChannelMessageFilter>> timer_;

  bool a_stub_is_descheduled_;

  // True if this channel can create future sync points.
  bool future_sync_points_;

  // This number is only ever incremented/read on the IO thread.
  static uint32_t global_order_counter_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_H_
