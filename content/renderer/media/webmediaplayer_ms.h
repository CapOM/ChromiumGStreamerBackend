// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/layers/video_frame_provider.h"
#include "media/blink/skcanvas_video_renderer.h"
#include "media/blink/webmediaplayer_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "url/gurl.h"

namespace blink {
class WebFrame;
class WebGraphicsContext3D;
class WebMediaPlayerClient;
}

namespace media {
class MediaLog;
class WebMediaPlayerDelegate;
}

namespace cc_blink {
class WebLayerImpl;
}

namespace content {
class MediaStreamAudioRenderer;
class MediaStreamRendererFactory;
class VideoFrameProvider;

// WebMediaPlayerMS delegates calls from WebCore::MediaPlayerPrivate to
// Chrome's media player when "src" is from media stream.
//
// WebMediaPlayerMS works with multiple objects, the most important ones are:
//
// VideoFrameProvider
//   provides video frames for rendering.
//
// TODO(wjia): add AudioPlayer.
// AudioPlayer
//   plays audio streams.
//
// blink::WebMediaPlayerClient
//   WebKit client of this media player object.
class WebMediaPlayerMS
    : public blink::WebMediaPlayer,
      public base::SupportsWeakPtr<WebMediaPlayerMS> {
 public:
  // Construct a WebMediaPlayerMS with reference to the client, and
  // a MediaStreamClient which provides VideoFrameProvider.
  WebMediaPlayerMS(blink::WebFrame* frame,
                   blink::WebMediaPlayerClient* client,
                   base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
                   media::MediaLog* media_log,
                   scoped_ptr<MediaStreamRendererFactory> factory,
                   const scoped_refptr<base::SingleThreadTaskRunner>&
                       compositor_task_runner);

  virtual ~WebMediaPlayerMS();

  virtual void load(LoadType load_type,
                    const blink::WebURL& url,
                    CORSMode cors_mode);

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual bool supportsSave() const;
  virtual void seek(double seconds);
  virtual void setRate(double rate);
  virtual void setVolume(double volume);
  virtual void setSinkId(const blink::WebString& device_id,
                         media::WebSetSinkIdCB* web_callback);
  virtual void setPreload(blink::WebMediaPlayer::Preload preload);
  virtual blink::WebTimeRanges buffered() const;
  virtual blink::WebTimeRanges seekable() const;

  // Methods for painting.
  virtual void paint(blink::WebCanvas* canvas,
                     const blink::WebRect& rect,
                     unsigned char alpha,
                     SkXfermode::Mode mode);

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const;
  virtual bool hasAudio() const;

  // Dimensions of the video.
  virtual blink::WebSize naturalSize() const;

  // Getters of playback state.
  virtual bool paused() const;
  virtual bool seeking() const;
  virtual double duration() const;
  virtual double currentTime() const;

  // Internal states of loading and network.
  virtual blink::WebMediaPlayer::NetworkState networkState() const;
  virtual blink::WebMediaPlayer::ReadyState readyState() const;

  virtual bool didLoadingProgress();

  virtual bool hasSingleSecurityOrigin() const;
  virtual bool didPassCORSAccessCheck() const;

  virtual double mediaTimeForTimeValue(double timeValue) const;

  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  bool copyVideoTextureToPlatformTexture(
      blink::WebGraphicsContext3D* web_graphics_context,
      unsigned int texture,
      unsigned int internal_format,
      unsigned int type,
      bool premultiply_alpha,
      bool flip_y) override;

 private:
  class Compositor : public cc::VideoFrameProvider {
   public:
    explicit Compositor(const scoped_refptr<base::SingleThreadTaskRunner>&
                            compositor_task_runner);
    ~Compositor() override;

    void EnqueueFrame(scoped_refptr<media::VideoFrame> const& frame);

    // Statistical data
    gfx::Size GetCurrentSize();
    base::TimeDelta GetCurrentTime();
    unsigned GetTotalFrameCount();
    unsigned GetDroppedFrameCount();

    // VideoFrameProvider implementation.
    void SetVideoFrameProviderClient(
        cc::VideoFrameProvider::Client* client) override;
    bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                            base::TimeTicks deadline_max) override;
    bool HasCurrentFrame() override;
    scoped_refptr<media::VideoFrame> GetCurrentFrame() override;
    void PutCurrentFrame() override;

    void StartRendering();
    void StopRendering();
    void ReplaceCurrentFrameWithACopy(media::SkCanvasVideoRenderer* renderer);

   private:
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

    // A pointer back to the compositor to inform it about state changes. This
    // is not NULL while the compositor is actively using this webmediaplayer.
    cc::VideoFrameProvider::Client* video_frame_provider_client_;

    // |current_frame_| is updated only on compositor thread. The object it
    // holds can be freed on the compositor thread if it is the last to hold a
    // reference but media::VideoFrame is a thread-safe ref-pointer. It is
    // however read on the compositor and main thread so locking is required
    // around all modifications and all reads on the any thread.
    scoped_refptr<media::VideoFrame> current_frame_;

    // |current_frame_used_| is updated on compositor thread only.
    // It's used to track whether |current_frame_| was painted for detecting
    // when to increase |dropped_frame_count_|.
    bool current_frame_used_;

    base::TimeTicks last_deadline_max_;
    unsigned total_frame_count_;
    unsigned dropped_frame_count_;

    bool paused_;

    // |current_frame_lock_| protects |current_frame_used_| and
    // |current_frame_|.
    base::Lock current_frame_lock_;
  };

  // The callback for VideoFrameProvider to signal a new frame is available.
  void OnFrameAvailable(const scoped_refptr<media::VideoFrame>& frame);
  // Need repaint due to state change.
  void RepaintInternal();

  // The callback for source to report error.
  void OnSourceError();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Getter method to |client_|.
  blink::WebMediaPlayerClient* GetClient();

  blink::WebFrame* const frame_;

  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  const blink::WebTimeRanges buffered_;

  float volume_;

  blink::WebMediaPlayerClient* const client_;

  const base::WeakPtr<media::WebMediaPlayerDelegate> delegate_;

  // Specify content:: to disambiguate from cc::.
  scoped_refptr<content::VideoFrameProvider> video_frame_provider_;
  bool paused_;
  bool remote_;

  scoped_ptr<cc_blink::WebLayerImpl> video_weblayer_;

  bool received_first_frame_;

  scoped_refptr<MediaStreamAudioRenderer> audio_renderer_;

  media::SkCanvasVideoRenderer video_renderer_;

  scoped_refptr<media::MediaLog> media_log_;

  scoped_ptr<MediaStreamRendererFactory> renderer_factory_;


  // Used for DCHECKs to ensure methods calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  // WebMediaPlayerMS owns the Compositor instance, but the destructions of
  // compositor should take place on Compositor Thread.
  scoped_ptr<Compositor> compositor_;

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;


  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMS);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_
