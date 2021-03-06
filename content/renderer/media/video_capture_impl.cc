// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Notes about usage of this object by VideoCaptureImplManager.
//
// VideoCaptureImplManager access this object by using a Unretained()
// binding and tasks on the IO thread. It is then important that
// VideoCaptureImpl never post task to itself. All operations must be
// synchronous.

#include "content/renderer/media/video_capture_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/common/media/video_capture_messages.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

namespace content {

namespace {

const int kUndefinedDeviceId = 0;

// This is called on an unknown thread when the VideoFrame destructor executes.
// As of this writing, this callback mechanism is the only interface in
// VideoFrame to provide the final value for |release_sync_point|.
// VideoCaptureImpl::DidFinishConsumingFrame() will read the value saved here,
// and pass it back to the IO thread to pass back to the host via the
// BufferReady IPC.
void SaveReleaseSyncPoint(uint32* storage, uint32 release_sync_point) {
  *storage = release_sync_point;
}

}  // namespace

// A holder of a memory-backed buffer and accessors to it.
class VideoCaptureImpl::ClientBuffer
    : public base::RefCountedThreadSafe<ClientBuffer> {
 public:
  ClientBuffer(scoped_ptr<base::SharedMemory> buffer, size_t buffer_size)
      : buffer_(buffer.Pass()), buffer_size_(buffer_size) {}

  base::SharedMemory* buffer() const { return buffer_.get(); }
  size_t buffer_size() const { return buffer_size_; }

 private:
  friend class base::RefCountedThreadSafe<ClientBuffer>;

  virtual ~ClientBuffer() {}

  const scoped_ptr<base::SharedMemory> buffer_;
  const size_t buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(ClientBuffer);
};

// A holder of a GpuMemoryBuffer-backed buffer, Map()ed on ctor and Unmap()ed on
// dtor. Creates and owns GpuMemoryBuffer instances.
class VideoCaptureImpl::ClientBuffer2
    : public base::RefCountedThreadSafe<ClientBuffer2> {
 public:
  ClientBuffer2(
      const std::vector<gfx::GpuMemoryBufferHandle>& client_handles,
      const gfx::Size& size)
      : handles_(client_handles),
        size_(size) {
    const media::VideoPixelFormat format = media::PIXEL_FORMAT_I420;
    for (size_t i = 0; i < handles_.size(); ++i) {
      const size_t width = media::VideoFrame::Columns(i, format, size_.width());
      const size_t height = media::VideoFrame::Rows(i, format, size_.height());
      buffers_.push_back(GpuMemoryBufferImpl::CreateFromHandle(
          handles_[i],
          gfx::Size(width, height),
          gfx::BufferFormat::R_8,
          gfx::BufferUsage::MAP,
          base::Bind(&ClientBuffer2::DestroyGpuMemoryBuffer,
                     base::Unretained(this))));
      void* data_ptr = nullptr;
      buffers_[i]->Map(&data_ptr);
      data_[i] = reinterpret_cast<uint8*>(data_ptr);
      strides_[i] = width;
    }
  }

  uint8* data(int plane) const { return data_[plane]; }
  int32 stride(int plane) const { return strides_[plane]; }
  std::vector<gfx::GpuMemoryBufferHandle> gpu_memory_buffer_handles() {
    return handles_;
  }

 private:
  friend class base::RefCountedThreadSafe<ClientBuffer2>;

  virtual ~ClientBuffer2() {
    for (auto& buffer : buffers_)
      buffer->Unmap();
  }

  void DestroyGpuMemoryBuffer(uint32 sync_point) {}

  const std::vector<gfx::GpuMemoryBufferHandle> handles_;
  const gfx::Size size_;
  ScopedVector<gfx::GpuMemoryBuffer> buffers_;
  uint8* data_[media::VideoFrame::kMaxPlanes];
  int32 strides_[media::VideoFrame::kMaxPlanes];

  DISALLOW_COPY_AND_ASSIGN(ClientBuffer2);
};

VideoCaptureImpl::VideoCaptureImpl(
    const media::VideoCaptureSessionId session_id,
    VideoCaptureMessageFilter* filter)
    : message_filter_(filter),
      device_id_(kUndefinedDeviceId),
      session_id_(session_id),
      state_(VIDEO_CAPTURE_STATE_STOPPED),
      weak_factory_(this) {
  DCHECK(filter);
}

VideoCaptureImpl::~VideoCaptureImpl() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

void VideoCaptureImpl::Init() {
  // For creating callbacks in unittest, this class may be constructed from a
  // different thread than the IO thread, e.g. wherever unittest runs on.
  // Therefore, this function should define the thread ownership.
#if DCHECK_IS_ON()
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
#endif
  message_filter_->AddDelegate(this);
}

void VideoCaptureImpl::DeInit() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (state_ == VIDEO_CAPTURE_STATE_STARTED)
    Send(new VideoCaptureHostMsg_Stop(device_id_));
  message_filter_->RemoveDelegate(this);
}

void VideoCaptureImpl::SuspendCapture(bool suspend) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(suspend ? static_cast<IPC::Message*>(
                     new VideoCaptureHostMsg_Pause(device_id_))
               : static_cast<IPC::Message*>(new VideoCaptureHostMsg_Resume(
                     device_id_, session_id_, client_params_)));
}

void VideoCaptureImpl::StartCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureStateUpdateCB& state_update_cb,
    const VideoCaptureDeliverFrameCB& deliver_frame_cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (state_update_cb.is_null() || deliver_frame_cb.is_null()) {
    DVLOG(1) << "StartCapture: Passed null callbacks.";
    return;
  }
  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    state_update_cb.Run(VIDEO_CAPTURE_STATE_ERROR);
    return;
  }

  // Save client info regardless of whether we're actually starting capture
  // now or not, in case a delegate hasn't yet been added.
  client_params_ = params;
  state_update_cb_ = state_update_cb;
  deliver_frame_cb_ = deliver_frame_cb;
  client_params_.requested_format.frame_rate =
      std::min(client_params_.requested_format.frame_rate,
               static_cast<float>(media::limits::kMaxFramesPerSecond));

  // Postpone starting capture until OnDelegateAdded(). Do this after saving
  // |client_params_|, etc. as StartCapture will be called with these saved
  // values as soon as a delegate has been added
  if (device_id_ == kUndefinedDeviceId) {
    DVLOG(1) << "StartCapture: Not starting, device_id_ is undefined.";
    return;
  }

  // Finally, we can start (assuming we haven't started already).
  // Notify the client that we have started regardless of |state_|.
  state_update_cb.Run(VIDEO_CAPTURE_STATE_STARTED);
  if (state_ == VIDEO_CAPTURE_STATE_STARTED)
    return;
  DVLOG(1) << "StartCapture: starting with resolution "
           << client_params_.requested_format.frame_size.ToString();
  StartCaptureInternal();
}

void VideoCaptureImpl::StopCapture() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (state_ == VIDEO_CAPTURE_STATE_STOPPED || !IsInitialized())
    return;

  DVLOG(1) << "StopCapture: Stopping capture.";
  state_update_cb_.Run(VIDEO_CAPTURE_STATE_STOPPED);
  StopDevice();
  client_buffers_.clear();
  client_buffer2s_.clear();
  ResetClient();
  weak_factory_.InvalidateWeakPtrs();
}

void VideoCaptureImpl::GetDeviceSupportedFormats(
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  device_formats_cb_queue_.push_back(callback);
  if (device_formats_cb_queue_.size() == 1)
    Send(new VideoCaptureHostMsg_GetDeviceSupportedFormats(device_id_,
                                                           session_id_));
}

void VideoCaptureImpl::GetDeviceFormatsInUse(
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  device_formats_in_use_cb_queue_.push_back(callback);
  if (device_formats_in_use_cb_queue_.size() == 1)
    Send(
        new VideoCaptureHostMsg_GetDeviceFormatsInUse(device_id_, session_id_));
}

void VideoCaptureImpl::OnBufferCreated(base::SharedMemoryHandle handle,
                                       int length,
                                       int buffer_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // In case client calls StopCapture before the arrival of created buffer,
  // just close this buffer and return.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED) {
    base::SharedMemory::CloseHandle(handle);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory(handle, false));
  if (!shm->Map(length)) {
    DLOG(ERROR) << "OnBufferCreated: Map failed.";
    return;
  }
  const bool inserted =
      client_buffers_.insert(std::make_pair(buffer_id, new ClientBuffer(
                                                           shm.Pass(), length)))
          .second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferCreated2(
    const std::vector<gfx::GpuMemoryBufferHandle>& handles,
    const gfx::Size& size,
    int buffer_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // In case client calls StopCapture before the arrival of created buffer,
  // just close this buffer and return.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;

  const bool inserted =
      client_buffer2s_.insert(std::make_pair(buffer_id,
                                             new ClientBuffer2(handles, size)))
          .second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferDestroyed(int buffer_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  const auto& cb_iter = client_buffers_.find(buffer_id);
  if (cb_iter != client_buffers_.end()) {
    DCHECK(!cb_iter->second.get() || cb_iter->second->HasOneRef())
        << "Instructed to delete buffer we are still using.";
    client_buffers_.erase(cb_iter);
  } else {
    const auto& cb2_iter = client_buffer2s_.find(buffer_id);
    if (cb2_iter != client_buffer2s_.end()) {
      DCHECK(!cb2_iter->second.get() || cb2_iter->second->HasOneRef())
          << "Instructed to delete buffer we are still using.";
      client_buffer2s_.erase(cb2_iter);
    }
  }
}

void VideoCaptureImpl::OnBufferReceived(
    int buffer_id,
    base::TimeTicks timestamp,
    const base::DictionaryValue& metadata,
    media::VideoPixelFormat pixel_format,
    media::VideoFrame::StorageType storage_type,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const std::vector<gpu::MailboxHolder>& mailbox_holders) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (state_ != VIDEO_CAPTURE_STATE_STARTED) {
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id, 0, -1.0));
    return;
  }
  if (first_frame_timestamp_.is_null())
    first_frame_timestamp_ = timestamp;

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "OnBufferReceived",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       timestamp.ToInternalValue(), "time_delta",
                       (timestamp - first_frame_timestamp_).ToInternalValue());

  scoped_refptr<media::VideoFrame> frame;
  base::Callback<void(uint32, double)> buffer_finished_callback;
  uint32* release_sync_point_storage = new uint32(0);
  if (mailbox_holders.empty()) {
    DCHECK_EQ(media::PIXEL_FORMAT_I420, pixel_format);
    const auto& iter = client_buffers_.find(buffer_id);
    DCHECK(iter != client_buffers_.end());
    const scoped_refptr<ClientBuffer> buffer = iter->second;
    frame = media::VideoFrame::WrapExternalSharedMemory(
        pixel_format,
        coded_size,
        visible_rect,
        gfx::Size(visible_rect.width(), visible_rect.height()),
        reinterpret_cast<uint8*>(buffer->buffer()->memory()),
        buffer->buffer_size(),
        buffer->buffer()->handle(),
        0 /* shared_memory_offset */,
        timestamp - first_frame_timestamp_);
    buffer_finished_callback = media::BindToCurrentLoop(
        base::Bind(&VideoCaptureImpl::OnClientBufferFinished,
                   weak_factory_.GetWeakPtr(), buffer_id, buffer));
  } else {
#if DCHECK_IS_ON()
    for (const auto& mailbox_holder : mailbox_holders)
      DCHECK(mailbox_holder.mailbox.Verify());
    DCHECK(mailbox_holders.size() == 1u || mailbox_holders.size() == 3u);
#endif

    scoped_refptr<ClientBuffer2> buffer;
    if (mailbox_holders.size() ==
        media::VideoFrame::NumPlanes(media::PIXEL_FORMAT_ARGB)) {
      DCHECK_EQ(media::PIXEL_FORMAT_ARGB, pixel_format);
      frame = media::VideoFrame::WrapNativeTexture(
          pixel_format,
          mailbox_holders[0],
          base::Bind(&SaveReleaseSyncPoint, release_sync_point_storage),
          coded_size,
          gfx::Rect(coded_size),
          coded_size,
          timestamp - first_frame_timestamp_);
    } else if (mailbox_holders.size() ==
               media::VideoFrame::NumPlanes(media::PIXEL_FORMAT_I420)) {
      DCHECK_EQ(media::PIXEL_FORMAT_I420, pixel_format);
      const auto& iter = client_buffer2s_.find(buffer_id);
      DCHECK(iter != client_buffer2s_.end());
      buffer = iter->second;
      frame = media::VideoFrame::WrapYUV420NativeTextures(
          mailbox_holders[media::VideoFrame::kYPlane],
          mailbox_holders[media::VideoFrame::kUPlane],
          mailbox_holders[media::VideoFrame::kVPlane],
          base::Bind(&SaveReleaseSyncPoint, release_sync_point_storage),
          coded_size,
          gfx::Rect(coded_size),
          coded_size,
          timestamp - first_frame_timestamp_);
    }
    buffer_finished_callback = media::BindToCurrentLoop(
        base::Bind(&VideoCaptureImpl::OnClientBufferFinished2,
                   weak_factory_.GetWeakPtr(), buffer_id, buffer));
  }
  frame->metadata()->SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                                  timestamp);
  frame->AddDestructionObserver(
      base::Bind(&VideoCaptureImpl::DidFinishConsumingFrame,
                 frame->metadata(), release_sync_point_storage,
                 buffer_finished_callback));
  frame->metadata()->MergeInternalValuesFrom(metadata);
  deliver_frame_cb_.Run(frame, timestamp);
}

void VideoCaptureImpl::OnClientBufferFinished(
    int buffer_id,
    const scoped_refptr<ClientBuffer>& /* ignored_buffer */,
    uint32 release_sync_point,
    double consumer_resource_utilization) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id,
                                           release_sync_point,
                                           consumer_resource_utilization));
}
void VideoCaptureImpl::OnClientBufferFinished2(
    int buffer_id,
    const scoped_refptr<ClientBuffer2>& gpu_memory_buffer /* ignored_buffer */,
    uint32 release_sync_point,
    double consumer_resource_utilization) {
  OnClientBufferFinished(buffer_id, scoped_refptr<ClientBuffer>(),
                         release_sync_point, consumer_resource_utilization);
}

void VideoCaptureImpl::OnStateChanged(VideoCaptureState state) {
  // TODO(ajose): http://crbug.com/522155 improve this state machine.
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  state_ = state;

  if (state == VIDEO_CAPTURE_STATE_STOPPED) {
    DVLOG(1) << "OnStateChanged: stopped!, device_id = " << device_id_;
    client_buffers_.clear();
    client_buffer2s_.clear();
    weak_factory_.InvalidateWeakPtrs();
    return;
  }
  if (state == VIDEO_CAPTURE_STATE_ERROR) {
    DVLOG(1) << "OnStateChanged: error!, device_id = " << device_id_;
    if (!state_update_cb_.is_null())
      state_update_cb_.Run(VIDEO_CAPTURE_STATE_ERROR);
    ResetClient();
    return;
  }
  if (state == VIDEO_CAPTURE_STATE_ENDED) {
    DVLOG(1) << "OnStateChanged: ended!, device_id = " << device_id_;
    // We'll only notify the client that the stream has stopped.
    if (!state_update_cb_.is_null())
      state_update_cb_.Run(VIDEO_CAPTURE_STATE_STOPPED);
    ResetClient();
    return;
  }
  NOTREACHED();
}

void VideoCaptureImpl::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& supported_formats) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (size_t i = 0; i < device_formats_cb_queue_.size(); ++i)
    device_formats_cb_queue_[i].Run(supported_formats);
  device_formats_cb_queue_.clear();
}

void VideoCaptureImpl::OnDeviceFormatsInUseReceived(
    const media::VideoCaptureFormats& formats_in_use) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (size_t i = 0; i < device_formats_in_use_cb_queue_.size(); ++i)
    device_formats_in_use_cb_queue_[i].Run(formats_in_use);
  device_formats_in_use_cb_queue_.clear();
}

void VideoCaptureImpl::OnDelegateAdded(int32 device_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "OnDelegateAdded: device_id " << device_id;

  device_id_ = device_id;
  StartCapture(client_params_, state_update_cb_, deliver_frame_cb_);
}

void VideoCaptureImpl::StopDevice() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    Send(new VideoCaptureHostMsg_Stop(device_id_));
    client_params_.requested_format.frame_size.SetSize(0, 0);
  }
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPED);

  DVLOG(1) << "RestartCapture, restarting capture.";
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(device_id_);

  Send(new VideoCaptureHostMsg_Start(device_id_, session_id_, client_params_));
  state_ = VIDEO_CAPTURE_STATE_STARTED;
}

void VideoCaptureImpl::Send(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  message_filter_->Send(message);
}

// static
void VideoCaptureImpl::DidFinishConsumingFrame(
    const media::VideoFrameMetadata* metadata,
    uint32* release_sync_point_storage,
    const base::Callback<void(uint32, double)>& callback_to_io_thread) {
  // Note: This function may be called on any thread by the VideoFrame
  // destructor.  |metadata| is still valid for read-access at this point.

  uint32 release_sync_point = 0u;
  if (release_sync_point_storage) {
    release_sync_point = *release_sync_point_storage;
    delete release_sync_point_storage;
  }

  double consumer_resource_utilization = -1.0;
  if (!metadata->GetDouble(media::VideoFrameMetadata::RESOURCE_UTILIZATION,
                           &consumer_resource_utilization)) {
    consumer_resource_utilization = -1.0;
  }

  callback_to_io_thread.Run(release_sync_point, consumer_resource_utilization);
}

bool VideoCaptureImpl::IsInitialized() const {
  return !(state_update_cb_.is_null() || deliver_frame_cb_.is_null()) &&
         device_id_ != kUndefinedDeviceId;
}

void VideoCaptureImpl::ResetClient() {
  client_params_ = media::VideoCaptureParams();
  state_update_cb_.Reset();
  deliver_frame_cb_.Reset();
  first_frame_timestamp_ = base::TimeTicks();
  device_id_ = kUndefinedDeviceId;
  state_ = VIDEO_CAPTURE_STATE_STOPPED;
}

}  // namespace content
